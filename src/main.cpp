#include <Geode/Geode.hpp>
#include <Geode/modify/LevelPage.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/binding/GameStatsManager.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include <algorithm>
#include <cmath>
#include <string>

using namespace geode::prelude;

namespace readiness {

struct Result {
    int score = 1;
    int experience = 0;
    int demand = 50;
    int normal = 0;
    int practice = 0;
    int levelAttempts = 0;
    int confidence = 50;
    bool rated = false;
    bool platformer = false;
    std::string difficultyLabel = "Sin rating";
    std::string verdict;
};

static double satLog(int value, double cap) {
    if (value <= 0 || cap <= 0.0) return 0.0;
    return std::clamp(std::log1p(static_cast<double>(value)) / std::log1p(cap), 0.0, 1.0);
}

static int demandFromLevel(GJGameLevel* level, std::string& label, bool& rated) {
    if (!level) {
        label = "Desconocida";
        rated = false;
        return 50;
    }

    if (level->m_autoLevel) {
        label = "Auto";
        rated = true;
        return 1;
    }

    const int stars = std::max(0, level->m_stars.value());
    const bool demon = level->m_demon.value() > 0 || stars >= 10;

    if (demon) {
        rated = true;
        // Valores usados por GD para el subtipo de Demon:
        // 3 Easy, 4 Medium, 0 Hard, 5 Insane, 6 Extreme.
        switch (level->m_demonDifficulty) {
            case 3:
                label = "Easy Demon";
                return 78;
            case 4:
                label = "Medium Demon";
                return 85;
            case 5:
                label = "Insane Demon";
                return 96;
            case 6:
                label = "Extreme Demon";
                return 100;
            default:
                label = "Hard Demon";
                return 90;
        }
    }

    if (stars > 0) {
        rated = true;
        switch (stars) {
            case 1: label = "Easy";   return 8;
            case 2: label = "Easy";   return 14;
            case 3: label = "Normal"; return 22;
            case 4: label = "Hard";   return 30;
            case 5: label = "Hard";   return 38;
            case 6: label = "Harder"; return 47;
            case 7: label = "Harder"; return 56;
            case 8: label = "Insane"; return 67;
            case 9: label = "Insane"; return 76;
            default: label = "Rated"; return std::clamp(8 + stars * 8, 8, 88);
        }
    }

    // Para niveles sin estrellas intentamos aprovechar el promedio de dificultad
    // si existe, pero mantenemos una confianza más baja.
    rated = false;
    const int avg = level->getAverageDifficulty();
    if (avg > 0) {
        if (avg <= 10) { label = "Easy (estimada)"; return 12; }
        if (avg <= 20) { label = "Normal (estimada)"; return 23; }
        if (avg <= 30) { label = "Hard (estimada)"; return 36; }
        if (avg <= 40) { label = "Harder (estimada)"; return 52; }
        label = "Insane (estimada)";
        return 70;
    }

    label = "Sin rating";
    return 48;
}

static int experienceScore() {
    auto* stats = GameStatsManager::get();
    if (!stats) return 0;

    const int jumps = std::max(0, stats->getStat("1"));
    const int attempts = std::max(0, stats->getStat("2"));
    const int officialCompleted = std::max(0, stats->getStat("3"));
    const int onlineCompleted = std::max(0, stats->getStat("4"));
    const int demons = std::max(0, stats->getStat("5"));
    const int stars = std::max(0, stats->getStat("6"));

    double score = 0.0;
    score += 36.0 * satLog(demons, 1000.0);
    score += 22.0 * satLog(stars, 25000.0);
    score += 16.0 * satLog(onlineCompleted, 5000.0);
    score += 10.0 * satLog(officialCompleted, 26.0);
    score += 8.0 * satLog(jumps, 2000000.0);
    score += 8.0 * satLog(attempts, 200000.0);

    return std::clamp(static_cast<int>(std::lround(score)), 0, 100);
}

static std::string makeVerdict(int score) {
    if (score >= 90) return "Estás muy preparado para este nivel.";
    if (score >= 75) return "Estás bastante preparado. Vale la pena intentarlo.";
    if (score >= 60) return "Tienes una base razonable, aunque puede costarte.";
    if (score >= 45) return "Estás a medio camino. Probablemente necesites práctica.";
    if (score >= 30) return "El nivel está por encima de tu experiencia actual.";
    return "Este nivel parece un salto grande para tus estadísticas actuales.";
}

static Result analyze(GJGameLevel* level) {
    Result r;
    if (!level) {
        r.verdict = "No se pudo leer este nivel.";
        return r;
    }

    r.experience = experienceScore();
    r.demand = demandFromLevel(level, r.difficultyLabel, r.rated);
    r.normal = std::clamp(level->getNormalPercent(), 0, 100);
    r.practice = std::clamp(level->m_practicePercent, 0, 100);
    r.levelAttempts = std::max(0, level->m_attempts.value());
    r.platformer = level->isPlatformer();

    if (r.normal >= 100) {
        r.score = 100;
        r.confidence = 100;
        r.verdict = "Ya completaste este nivel.";
        return r;
    }

    // El 62 es el punto de partida. La diferencia entre experiencia y exigencia
    // mueve la mayor parte del resultado; el progreso real en ESTE nivel puede
    // corregir bastante una estimación basada solo en estadísticas globales.
    double score = 62.0 + (static_cast<double>(r.experience - r.demand) * 0.75);
    score += static_cast<double>(r.normal) * 0.32;
    score += static_cast<double>(r.practice) * 0.06;

    // Familiaridad: muchos intentos en el nivel aportan un pequeño bonus, pero
    // nunca pueden convertir por sí solos un nivel demasiado difícil en 100/100.
    score += 6.0 * satLog(r.levelAttempts, 1000.0);

    if (r.platformer) {
        // En platformer el porcentaje clásico es menos representativo.
        score -= 3.0;
    }

    r.score = std::clamp(static_cast<int>(std::lround(score)), 1, 100);

    int confidence = 42;
    if (r.rated) confidence += 25;
    confidence += static_cast<int>(std::lround(18.0 * satLog(r.levelAttempts, 500.0)));

    auto* stats = GameStatsManager::get();
    if (stats) {
        int history = std::max(0, stats->getStat("4")) + std::max(0, stats->getStat("5"));
        confidence += static_cast<int>(std::lround(10.0 * satLog(history, 1000.0)));
    }

    if (r.platformer) confidence -= 12;
    r.confidence = std::clamp(confidence, 30, 95);
    r.verdict = makeVerdict(r.score);
    return r;
}

static std::string scoreColor(int score) {
    if (score >= 75) return "<cg>";
    if (score >= 45) return "<cy>";
    return "<cr>";
}

static void show(GJGameLevel* level) {
    if (!level) {
        FLAlertLayer::create("Level Readiness", "No pude obtener los datos del nivel.", "OK")->show();
        return;
    }

    auto r = analyze(level);
    const auto color = scoreColor(r.score);

    std::string body = fmt::format(
        "<cj><cb>Preparación:</c> {}{}/100</c></cj>\n\n"
        "<cy>Dificultad:</c> {}\n"
        "<cy>Experiencia:</c> {}/100\n"
        "<cy>Exigencia:</c> {}/100\n"
        "<cy>Progreso:</c> {}%  <cy>Práctica:</c> {}%\n"
        "<cy>Intentos aquí:</c> {}\n"
        "<cy>Confianza:</c> {}%\n\n"
        "{}\n\n"
        "<cp>Índice heurístico: no es una probabilidad matemática garantizada.</c>",
        color,
        r.score,
        r.difficultyLabel,
        r.experience,
        r.demand,
        r.normal,
        r.practice,
        r.levelAttempts,
        r.confidence,
        r.verdict
    );

    FLAlertLayer::create(level->m_levelName.c_str(), body.c_str(), "OK")->show();
}

static CCMenuItemSpriteExtra* makeButton(CCObject* target, SEL_MenuHandler selector) {
    auto* sprite = ButtonSprite::create("?");
    sprite->setScale(0.62f);
    auto* button = CCMenuItemSpriteExtra::create(sprite, target, selector);
    button->setID("level-readiness-button");
    return button;
}

static CCMenu* makeOverlayMenu() {
    auto* menu = CCMenu::create();
    menu->setPosition(CCPointZero);
    menu->setID("level-readiness-menu");
    return menu;
}

} // namespace readiness

class $modify(LevelReadinessMainPage, LevelPage) {
    bool init(GJGameLevel* level) {
        if (!LevelPage::init(level)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto* menu = readiness::makeOverlayMenu();
        auto* button = readiness::makeButton(
            this,
            menu_selector(LevelReadinessMainPage::onLevelReadiness)
        );

        // Arriba a la derecha del menú de niveles principales.
        button->setPosition({winSize.width - 30.f, winSize.height - 28.f});
        menu->addChild(button);
        this->addChild(menu, 1000);
        return true;
    }

    void onLevelReadiness(CCObject*) {
        readiness::show(m_level);
    }
};

class $modify(LevelReadinessCommunity, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto* menu = readiness::makeOverlayMenu();
        auto* button = readiness::makeButton(
            this,
            menu_selector(LevelReadinessCommunity::onLevelReadiness)
        );

        // Node IDs llama a esta etiqueta "title-label". Si algún mod cambia la
        // jerarquía, usamos una posición de respaldo cerca del título.
        CCPoint pos {winSize.width / 2.f + 118.f, winSize.height - 25.f};
        if (auto* title = this->getChildByID("title-label")) {
            auto* parent = title->getParent();
            if (parent) {
                auto centerWorld = parent->convertToWorldSpace(title->getPosition());
                auto centerLocal = this->convertToNodeSpace(centerWorld);
                float halfWidth = title->getContentSize().width * std::abs(title->getScaleX()) * 0.5f;
                pos.x = std::clamp(centerLocal.x + halfWidth + 18.f, 28.f, winSize.width - 28.f);
                pos.y = std::clamp(centerLocal.y, 25.f, winSize.height - 25.f);
            }
        }

        button->setPosition(pos);
        menu->addChild(button);
        this->addChild(menu, 1000);
        return true;
    }

    void onLevelReadiness(CCObject*) {
        readiness::show(m_level);
    }
};
