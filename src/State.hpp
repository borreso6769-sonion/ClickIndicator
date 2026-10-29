#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

using namespace geode::prelude;

namespace ci {

    // Un click: x1 = posicion X del jugador al presionar (linea blanca),
    // x2 = posicion X al soltar (fin de la barra verde).
    struct Click {
        float x1;
        float x2;
    };

    struct Saved {
        std::string name;
        std::filesystem::path path;
    };

    inline bool recording = false;    // grabando clicks ahora mismo
    inline bool holding = false;      // hay un click presionado en la grabacion
    inline bool usingSaved = false;   // mostrando el indicador
    inline std::vector<Click> current;  // grabacion en curso
    inline std::vector<Click> loaded;   // grabacion seleccionada (la que se dibuja)
    inline std::string selectedName;
    inline std::filesystem::path selectedPath;

    inline void alertMsg(char const* title, std::string const& msg) {
        FLAlertLayer::create(title, msg, "OK")->show();
    }

    // =====================================================================
    //  Archivos
    // =====================================================================

    inline std::filesystem::path recordingsDir() {
        auto dir = Mod::get()->getSaveDir() / "recordings";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    inline std::string sanitize(std::string const& s) {
        std::string out;
        for (char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') out.push_back(c);
            else if (c == ' ') out.push_back('_');
        }
        return out;
    }

    inline std::string levelKey(PlayLayer* pl = nullptr) {
        if (!pl) pl = PlayLayer::get();
        if (!pl || !pl->m_level) return "";
        int id = pl->m_level->m_levelID.value();
        if (id > 0) return std::to_string(id);
        return "local_" + std::string(pl->m_level->m_levelName);
    }

    // Formato: linea 1 = clave del nivel, linea 2 = nombre, luego "x1 x2" por linea.
    inline bool readRecording(std::filesystem::path const& p, std::string& key, std::string& name, std::vector<Click>* clicks) {
        std::ifstream in(p);
        if (!in) return false;
        if (!std::getline(in, key)) return false;
        if (!std::getline(in, name)) return false;
        if (clicks) {
            clicks->clear();
            float a, b;
            while (in >> a >> b) clicks->push_back({a, b});
        }
        return true;
    }

    inline std::vector<Saved> listForLevel() {
        std::vector<Saved> res;
        auto key = levelKey();
        std::error_code ec;
        for (auto const& e : std::filesystem::directory_iterator(recordingsDir(), ec)) {
            if (e.path().extension() != ".txt") continue;
            std::string k, n;
            if (readRecording(e.path(), k, n, nullptr) && k == key) res.push_back({n, e.path()});
        }
        std::sort(res.begin(), res.end(), [](Saved const& a, Saved const& b) { return a.name < b.name; });
        return res;
    }

    // =====================================================================
    //  Ajustes (se guardan solos)
    // =====================================================================

    inline float getOpacity()    { return Mod::get()->getSavedValue<float>("green-opacity", 0.30f); }
    inline bool  getShowTower()  { return Mod::get()->getSavedValue<bool>("show-tower", true); }
    inline bool  getShowLine()   { return Mod::get()->getSavedValue<bool>("show-player-line", true); }
    inline bool  getShowJudge()  { return Mod::get()->getSavedValue<bool>("show-judge", true); }
    inline bool  getShowStats()  { return Mod::get()->getSavedValue<bool>("show-stats", true); }
    inline float getTowerScale() { return Mod::get()->getSavedValue<float>("tower-scale", 1.0f); }
    inline float getWarnBlocks() { return Mod::get()->getSavedValue<float>("warn-blocks", 15.0f); }

    // =====================================================================
    //  Temas
    // =====================================================================

    struct Col { float r, g, b; };

    struct Theme {
        char const* name;
        char const* desc;
        Col tap;        // color de los clicks simples
        Col hold;       // color de los holds
        Col rail;       // color de la torre
        float glow;     // brillo extra (0 = sin brillo)
        bool numbers;   // numerar los clicks
        float lineW;    // grosor (mitad) de la linea blanca
    };

    inline constexpr int THEME_COUNT = 4;

    inline Theme const& themeAt(int i) {
        static Theme const T[THEME_COUNT] = {
            {"Clasico",  "El de siempre: cuadros verdes.",
             {0.30f, 1.00f, 0.40f}, {0.15f, 1.00f, 0.25f}, {0.30f, 1.00f, 0.40f}, 0.0f, false, 1.5f},
            {"Neon",     "Brillo azul y rosa, muy vistoso.",
             {0.25f, 0.90f, 1.00f}, {1.00f, 0.30f, 0.90f}, {0.40f, 0.80f, 1.00f}, 1.0f, false, 1.5f},
            {"Tecnico",  "Clicks amarillos, holds verdes, numerados y lineas gruesas.",
             {1.00f, 0.85f, 0.20f}, {0.20f, 1.00f, 0.40f}, {0.85f, 0.85f, 0.85f}, 0.6f, true, 2.4f},
            {"Dorado",   "Elegante: dorado con brillo.",
             {1.00f, 0.82f, 0.25f}, {1.00f, 0.70f, 0.15f}, {1.00f, 0.85f, 0.40f}, 0.7f, false, 1.5f},
        };
        return T[std::clamp(i, 0, THEME_COUNT - 1)];
    }

    inline int themeIndex() { return Mod::get()->getSavedValue<int>("theme", 0); }
    inline Theme const& theme() { return themeAt(themeIndex()); }

    // =====================================================================
    //  Elementos visuales
    // =====================================================================

    // CCDrawNode mezcla con alfa PREMULTIPLICADO: si no multiplicamos el color por el alfa,
    // un alfa de 0 sigue viendose. Este helper lo hace bien.
    inline ccColor4F pm(float r, float g, float b, float a) {
        a = std::clamp(a, 0.f, 1.f);
        return {r * a, g * a, b * a, a};
    }
    inline ccColor4F pmc(Col c, float a) { return pm(c.r, c.g, c.b, a); }

    inline constexpr float LH = 180.f;  // alto del carril
    inline constexpr float LW = 56.f;   // ancho del carril

    inline Ref<CCNode> overlayRoot;               // contiene las guias (una por click)
    inline std::vector<Ref<CCDrawNode>> guideNodes;
    inline Ref<CCDrawNode> lineNode;              // linea blanca pegada al jugador
    inline Ref<CCDrawNode> towerNode;             // torre (HUD)
    inline CCLabelBMFont* judgeLabel = nullptr;
    inline CCLabelBMFont* subLabel = nullptr;
    inline CCLabelBMFont* statLabel = nullptr;
    inline std::vector<CCLabelBMFont*> numLabels;

    inline void clearVisuals() {
        guideNodes.clear();
        if (overlayRoot) overlayRoot->removeFromParent();
        overlayRoot = nullptr;
        if (lineNode) lineNode->removeFromParent();
        lineNode = nullptr;
        if (towerNode) towerNode->removeFromParent();
        towerNode = nullptr;
        judgeLabel = nullptr;
        subLabel = nullptr;
        statLabel = nullptr;
        numLabels.clear();
    }

    // =====================================================================
    //  Juicios: Perfecto / Bien / Mal
    // =====================================================================

    // Distancias en unidades del nivel (1 bloque = 30 unidades)
    inline constexpr float PERFECT_RANGE = 9.f;    // 0.3 bloques
    inline constexpr float GOOD_RANGE = 30.f;      // 1 bloque
    inline constexpr float BAD_RANGE = 75.f;       // 2.5 bloques (mas alla se ignora / cuenta como saltado)
    inline constexpr float RELEASE_RANGE = 30.f;   // tolerancia al soltar un hold

    inline size_t nextIdx = 0;     // siguiente click por juzgar
    inline int holdIdx = -1;       // hold que estas manteniendo
    inline bool userHold = false;
    inline int combo = 0;
    inline int statCount = 0;
    inline float statSum = 0.f;
    inline float flash = 0.f;      // destello de acierto

    inline void updateStats() {
        if (!statLabel) return;
        if (!getShowStats() || statCount == 0) {
            statLabel->setString("");
            return;
        }
        float acc = statSum / static_cast<float>(statCount) * 100.f;
        statLabel->setString(fmt::format("Combo x{} - {:.1f}%", combo, acc).c_str());
    }

    inline void showJudge(char const* text, ccColor3B col, char const* sub = nullptr, ccColor3B subCol = {255, 255, 255}) {
        if (!judgeLabel || !getShowJudge()) return;

        judgeLabel->stopAllActions();
        judgeLabel->setString(text);
        judgeLabel->setColor(col);
        judgeLabel->setOpacity(255);
        judgeLabel->setScale(0.8f);
        judgeLabel->runAction(CCSequence::create(
            CCScaleTo::create(0.10f, 0.5f), CCDelayTime::create(0.45f), CCFadeOut::create(0.25f), nullptr
        ));

        if (subLabel) {
            subLabel->stopAllActions();
            subLabel->setString(sub ? sub : "");
            subLabel->setColor(subCol);
            subLabel->setOpacity(255);
            subLabel->setScale(0.32f);
            subLabel->runAction(CCSequence::create(
                CCDelayTime::create(0.55f), CCFadeOut::create(0.25f), nullptr
            ));
        }
    }

    inline void registerResult(int kind, float d) {
        // kind: 0 = Perfecto, 1 = Bien, 2 = Mal
        char const* sub = nullptr;
        ccColor3B subCol = {255, 255, 255};
        if (kind != 0) {
            if (d < 0.f) { sub = "Temprano"; subCol = {110, 180, 255}; }
            else         { sub = "Tarde";    subCol = {255, 165, 60}; }
        }
        statCount++;
        if (kind == 0) {
            statSum += 1.f; combo++; flash = 1.f;
            showJudge("Perfecto!", {255, 225, 80}, sub, subCol);
        } else if (kind == 1) {
            statSum += 0.7f; combo++; flash = 0.7f;
            showJudge("Bien!", {90, 255, 110}, sub, subCol);
        } else {
            combo = 0;
            showJudge("Mal!", {255, 70, 70}, sub, subCol);
        }
        updateStats();
    }

    inline void resetJudge(float px = 0.f) {
        combo = 0;
        statCount = 0;
        statSum = 0.f;
        flash = 0.f;
        userHold = false;
        holdIdx = -1;
        nextIdx = 0;
        while (nextIdx < loaded.size() && px - loaded[nextIdx].x1 > BAD_RANGE) nextIdx++;
        updateStats();
    }

    // Se llama cuando el jugador PRESIONA click
    inline void judgePress(float px) {
        if (!usingSaved || loaded.empty()) return;
        userHold = true;
        if (nextIdx >= loaded.size()) return;

        auto const& c = loaded[nextIdx];
        float d = px - c.x1;                 // negativo = temprano, positivo = tarde
        if (d < -BAD_RANGE) return;          // todavia falta mucho: no es para este click

        float ad = std::abs(d);
        int kind = ad <= PERFECT_RANGE ? 0 : (ad <= GOOD_RANGE ? 1 : 2);
        registerResult(kind, d);

        holdIdx = (c.x2 - c.x1 > 3.f) ? static_cast<int>(nextIdx) : -1;
        nextIdx++;
    }

    // Se llama cuando el jugador SUELTA click
    inline void judgeRelease(float px) {
        userHold = false;
        if (!usingSaved || holdIdx < 0 || holdIdx >= static_cast<int>(loaded.size())) return;

        float d = px - loaded[holdIdx].x2;
        if (d < -RELEASE_RANGE) {
            combo = 0;
            showJudge("Soltaste antes!", {255, 140, 60});
            updateStats();
        }
        holdIdx = -1;
    }

    // =====================================================================
    //  Carril (torre) con cuadros que caen
    // =====================================================================

    inline float sc(float y) { return 1.f - 0.30f * (y / LH); }  // perspectiva: arriba mas angosto

    inline void quad(CCDrawNode* n, float xl0, float xr0, float y0, float xl1, float xr1, float y1,
                     ccColor4F f, float bw, ccColor4F b) {
        CCPoint p[4] = {{xl0, y0}, {xr0, y0}, {xr1, y1}, {xl1, y1}};
        n->drawPolygon(p, 4, f, bw, b);
    }

    inline void trap(CCDrawNode* n, float y0, float y1, float halfW, ccColor4F f, float bw, ccColor4F b, float grow = 0.f) {
        float w0 = halfW * sc(y0) + grow;
        float w1 = halfW * sc(y1) + grow;
        quad(n, -w0, w0, y0 - grow, -w1, w1, y1 + grow, f, bw, b);
    }

    inline void drawLane(float px) {
        if (!towerNode) return;
        auto const& T = theme();
        towerNode->clear();

        float warn = std::max(1.f, getWarnBlocks()) * 30.f; // 1 bloque = 30 unidades
        ccColor4F clear = {0.f, 0.f, 0.f, 0.f};
        float nw = LW / 2.f - 5.f;

        // Fondo del carril
        trap(towerNode, 0.f, LH, LW / 2.f, pm(0.f, 0.f, 0.f, 0.42f), 1.5f, pmc(T.rail, 0.55f));

        // Escalones que se desplazan hacia abajo (da sensacion de movimiento)
        float off = std::fmod(px / warn * LH, 22.f);
        for (int k = 0; k < 10; k++) {
            float y = 22.f * k - off;
            if (y <= 1.f || y >= LH - 1.f) continue;
            float w = (LW / 2.f) * sc(y) - 2.f;
            towerNode->drawSegment({-w, y}, {w, y}, 1.2f, pmc(T.rail, 0.25f));
        }

        // Destello al acertar
        if (flash > 0.f) {
            trap(towerNode, -4.f, 16.f, LW / 2.f, pm(1.f, 1.f, 1.f, flash * 0.35f), 0.f, clear, 3.f);
        }

        bool active = false;
        size_t shown = 0;

        for (size_t i = 0; i < loaded.size(); i++) {
            auto const& c = loaded[i];
            if (px > c.x2) continue;               // ya paso
            if (c.x1 - px > warn) break;           // todavia muy lejos (estan ordenados)

            bool hold = (c.x2 - c.x1) > 3.f;
            if (hold && px >= c.x1) active = true;

            float yBot = std::clamp(std::max(0.f, c.x1 - px) / warn * LH, 0.f, LH);
            float yTop = std::clamp((c.x2 - px) / warn * LH, 0.f, LH);
            Col col = hold ? T.hold : T.tap;

            // Cola del hold (cuanto mantener)
            if (hold && yTop - yBot > 0.5f) {
                if (T.glow > 0.f) trap(towerNode, yBot, yTop, nw, pmc(col, 0.18f * T.glow), 0.f, clear, 3.f);
                trap(towerNode, yBot, yTop, nw * 0.8f, pmc(col, 0.55f), 1.f, pmc(col, 0.95f));
            }

            // Cabeza: el cuadro que debes "tocar" con la linea blanca
            if (c.x1 >= px) {
                float hgt = 16.f * sc(yBot);
                float yHead = yBot + hgt;
                if (T.glow > 0.f) trap(towerNode, yBot, yHead, nw, pmc(col, 0.20f * T.glow), 0.f, clear, 3.5f);
                trap(towerNode, yBot, yHead, nw, pmc(col, 0.88f), 1.5f, pm(1.f, 1.f, 1.f, 0.9f));
                // Reflejo
                trap(towerNode, yBot + hgt * 0.55f, yHead - 1.f, nw * 0.8f, pm(1.f, 1.f, 1.f, 0.22f), 0.f, clear);
                // Tapa blanca = inicio exacto del click
                trap(towerNode, yBot, yBot + 3.5f * sc(yBot), nw, pm(1.f, 1.f, 1.f, 0.95f), 0.f, clear);

                // Numero (tema Tecnico)
                if (T.numbers && shown < numLabels.size()) {
                    auto lbl = numLabels[shown++];
                    lbl->setString(std::to_string(i + 1).c_str());
                    lbl->setPosition({0.f, yBot + hgt * 0.5f + 1.f});
                    lbl->setScale(0.30f * sc(yBot));
                    lbl->setVisible(true);
                }
            }
        }
        for (size_t k = shown; k < numLabels.size(); k++) numLabels[k]->setVisible(false);

        // Linea de impacto: aqui debe llegar el cuadro para clickear
        auto hit = active ? pmc(T.tap, 1.f) : pm(1.f, 1.f, 1.f, 0.92f);
        towerNode->drawSegment({-LW / 2 - 6.f, 0.f}, {LW / 2 + 6.f, 0.f}, active ? 3.2f : 2.f, hit);

        // Chevrones laterales ">  <" (senalan la linea de impacto)
        auto ch = pmc(T.rail, 0.95f);
        towerNode->drawSegment({-LW / 2 - 34.f, 13.f}, {-LW / 2 - 16.f, 0.f}, 3.f, ch);
        towerNode->drawSegment({-LW / 2 - 16.f, 0.f}, {-LW / 2 - 34.f, -13.f}, 3.f, ch);
        towerNode->drawSegment({LW / 2 + 34.f, 13.f}, {LW / 2 + 16.f, 0.f}, 3.f, ch);
        towerNode->drawSegment({LW / 2 + 16.f, 0.f}, {LW / 2 + 34.f, -13.f}, 3.f, ch);
    }

    // =====================================================================
    //  Crear / actualizar lo que se dibuja
    // =====================================================================

    inline void refreshOverlay() {
        clearVisuals();

        auto pl = PlayLayer::get();
        if (!pl || !pl->m_objectLayer) return;
        if (!usingSaved || loaded.empty()) return;

        auto const& T = theme();
        constexpr float bottom = -500.f;
        constexpr float top = 4000.f;
        ccColor4F clear = {0.f, 0.f, 0.f, 0.f};

        overlayRoot = CCNode::create();
        pl->m_objectLayer->addChild(overlayRoot, 1000);

        float op = std::clamp(getOpacity(), 0.f, 1.f);

        for (auto const& c : loaded) {
            auto node = CCDrawNode::create();
            bool hold = (c.x2 - c.x1) > 0.5f;
            Col col = hold ? T.hold : T.tap;

            // Barra: cuanto hay que mantener el click
            if (op > 0.01f && hold) {
                CCPoint g[4] = {{c.x1, bottom}, {c.x2, bottom}, {c.x2, top}, {c.x1, top}};
                node->drawPolygon(g, 4, pmc(col, op), 1.f, pmc(col, op * 1.6f));
            }

            // Aureola de color alrededor de la linea blanca (temas con brillo / tecnico)
            if (T.glow > 0.f) {
                float h = T.lineW * 3.2f;
                CCPoint a[4] = {{c.x1 - h, bottom}, {c.x1 + h, bottom}, {c.x1 + h, top}, {c.x1 - h, top}};
                node->drawPolygon(a, 4, pmc(col, 0.30f * T.glow), 0.f, clear);
            }

            // Linea blanca: donde empezar el click
            float lw = T.lineW;
            CCPoint w[4] = {{c.x1 - lw, bottom}, {c.x1 + lw, bottom}, {c.x1 + lw, top}, {c.x1 - lw, top}};
            node->drawPolygon(w, 4, pm(1.f, 1.f, 1.f, 0.95f), 0.f, clear);

            overlayRoot->addChild(node);
            guideNodes.push_back(node);
        }

        // Linea blanca del jugador (se junta con la de cada click)
        if (getShowLine()) {
            lineNode = CCDrawNode::create();
            CCPoint l[4] = {{-1.5f, bottom}, {1.5f, bottom}, {1.5f, top}, {-1.5f, top}};
            lineNode->drawPolygon(l, 4, pm(1.f, 1.f, 1.f, 1.f), 0.f, clear);
            pl->m_objectLayer->addChild(lineNode, 1001);
        }

        // Carril en la parte izquierda de la pantalla
        if (getShowTower() && pl->m_uiLayer) {
            auto win = CCDirector::get()->getWinSize();
            towerNode = CCDrawNode::create();
            towerNode->setPosition({win.width * 0.12f, win.height * 0.20f});
            towerNode->setScale(std::clamp(getTowerScale(), 0.4f, 2.f));

            judgeLabel = CCLabelBMFont::create("", "bigFont.fnt");
            judgeLabel->setPosition({0.f, -30.f});
            judgeLabel->setScale(0.5f);
            towerNode->addChild(judgeLabel, 5);

            subLabel = CCLabelBMFont::create("", "bigFont.fnt");
            subLabel->setPosition({0.f, -47.f});
            subLabel->setScale(0.32f);
            towerNode->addChild(subLabel, 5);

            statLabel = CCLabelBMFont::create("", "bigFont.fnt");
            statLabel->setPosition({0.f, -63.f});
            statLabel->setScale(0.34f);
            statLabel->setOpacity(210);
            towerNode->addChild(statLabel, 5);

            if (T.numbers) {
                for (int k = 0; k < 8; k++) {
                    auto lbl = CCLabelBMFont::create("", "bigFont.fnt");
                    lbl->setColor({15, 15, 15});
                    lbl->setVisible(false);
                    towerNode->addChild(lbl, 6);
                    numLabels.push_back(lbl);
                }
            }

            pl->m_uiLayer->addChild(towerNode, 100);
            drawLane(pl->m_player1 ? pl->m_player1->getPositionX() : 0.f);
            updateStats();
        }
    }

    // Se llama en cada frame
    inline void updateVisuals(PlayLayer* pl, float dt) {
        if (!usingSaved || loaded.empty() || !pl || !pl->m_player1) return;
        float px = pl->m_player1->getPositionX();

        // Cada guia desaparece cuando el jugador llega al final de su barra
        size_t n = std::min(guideNodes.size(), loaded.size());
        for (size_t i = 0; i < n; i++) {
            guideNodes[i]->setVisible(px <= loaded[i].x2);
        }

        if (lineNode) lineNode->setPositionX(px);

        // Clicks que se saltaron
        while (nextIdx < loaded.size() && px - loaded[nextIdx].x1 > BAD_RANGE) {
            registerResult(2, 1.f);
            nextIdx++;
        }

        // Mantuviste un hold demasiado tiempo
        if (userHold && holdIdx >= 0 && holdIdx < static_cast<int>(loaded.size())
            && px - loaded[holdIdx].x2 > RELEASE_RANGE) {
            combo = 0;
            showJudge("Soltaste tarde!", {255, 140, 60});
            updateStats();
            holdIdx = -1;
        }

        if (flash > 0.f) flash = std::max(0.f, flash - dt * 3.5f);

        if (towerNode) drawLane(px);
    }

    // =====================================================================
    //  Operaciones
    // =====================================================================

    // Recuerda la grabacion elegida para este nivel (asi no hay que volver a elegirla)
    inline void rememberSelection() {
        auto key = levelKey();
        if (key.empty()) return;
        Mod::get()->setSavedValue<std::string>("last-" + key,
            selectedPath.empty() ? std::string() : selectedPath.filename().string());
    }

    inline void rememberUse() {
        auto key = levelKey();
        if (key.empty()) return;
        Mod::get()->setSavedValue<bool>("last-use-" + key, usingSaved);
    }

    // Al entrar a un nivel: vuelve a cargar la ultima grabacion que usaste en el
    inline void autoLoad(PlayLayer* pl) {
        auto key = levelKey(pl);
        if (key.empty()) return;
        auto name = Mod::get()->getSavedValue<std::string>("last-" + key, "");
        if (name.empty()) return;

        auto p = recordingsDir() / name;
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) return;

        std::string k, n;
        std::vector<Click> clicks;
        if (!readRecording(p, k, n, &clicks) || k != key || clicks.empty()) return;

        loaded = clicks;
        selectedName = n;
        selectedPath = p;
        usingSaved = Mod::get()->getSavedValue<bool>("last-use-" + key, false);
    }

    inline bool saveCurrent(std::string const& name) {
        if (current.empty()) return false;
        auto key = levelKey();
        auto safe = sanitize(name);
        if (safe.empty()) safe = "rec";
        auto file = recordingsDir() / (sanitize(key) + "_" + safe + ".txt");

        std::ofstream out(file, std::ios::trunc);
        if (!out) return false;
        out << std::setprecision(9);
        out << key << "\n" << name << "\n";
        for (auto const& c : current) out << c.x1 << " " << c.x2 << "\n";
        out.close();

        selectedName = name;
        selectedPath = file;
        loaded = current;
        rememberSelection();
        if (usingSaved) refreshOverlay();
        return true;
    }

    inline bool selectRecording(Saved const& s) {
        std::string k, n;
        std::vector<Click> clicks;
        if (!readRecording(s.path, k, n, &clicks)) return false;
        loaded = clicks;
        selectedName = s.name;
        selectedPath = s.path;
        rememberSelection();
        if (usingSaved) {
            refreshOverlay();
            if (auto pl = PlayLayer::get(); pl && pl->m_player1) resetJudge(pl->m_player1->getPositionX());
        }
        return true;
    }

    inline void deleteSelected() {
        std::error_code ec;
        if (!selectedPath.empty()) std::filesystem::remove(selectedPath, ec);
        selectedName.clear();
        selectedPath.clear();
        loaded.clear();
        usingSaved = false;
        rememberSelection();
        rememberUse();
        refreshOverlay();
    }

    inline void resetAll() {
        clearVisuals();
        recording = false;
        holding = false;
        usingSaved = false;
        current.clear();
        loaded.clear();
        selectedName.clear();
        selectedPath.clear();
        nextIdx = 0;
        holdIdx = -1;
        userHold = false;
        combo = 0;
        statCount = 0;
        statSum = 0.f;
        flash = 0.f;
    }

    // =====================================================================
    //  Grabacion
    // =====================================================================

    inline void onPress(float x) {
        if (!recording || holding) return;
        current.push_back({x, x});
        holding = true;
    }

    inline void onRelease(float x) {
        if (!holding) return;
        if (!current.empty()) current.back().x2 = x;
        holding = false;
    }

    // Al morir / reaparecer en un checkpoint: borra los clicks de "el futuro".
    inline void truncateAt(float px) {
        if (!recording) return;
        while (!current.empty() && current.back().x1 >= px) current.pop_back();
        if (!current.empty() && current.back().x2 > px) current.back().x2 = px;
        holding = false;
    }

}
