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

    // ---------- Archivos ----------

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

    inline std::string levelKey() {
        auto pl = PlayLayer::get();
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

    // ---------- Ajustes (se guardan solos) ----------

    inline float getOpacity()    { return Mod::get()->getSavedValue<float>("green-opacity", 0.30f); }
    inline bool  getShowTower()  { return Mod::get()->getSavedValue<bool>("show-tower", true); }
    inline bool  getShowLine()   { return Mod::get()->getSavedValue<bool>("show-player-line", true); }
    inline float getTowerScale() { return Mod::get()->getSavedValue<float>("tower-scale", 1.0f); }
    inline float getWarnBlocks() { return Mod::get()->getSavedValue<float>("warn-blocks", 15.0f); }

    // ---------- Elementos visuales ----------

    inline Ref<CCNode> overlayRoot;               // contiene las guias (una por click)
    inline std::vector<Ref<CCDrawNode>> guideNodes;
    inline Ref<CCDrawNode> lineNode;              // linea blanca pegada al jugador
    inline Ref<CCDrawNode> towerNode;             // torre verde (HUD)
    inline float lastFrac = -1.f;

    inline void clearVisuals() {
        guideNodes.clear();
        if (overlayRoot) overlayRoot->removeFromParent();
        overlayRoot = nullptr;
        if (lineNode) lineNode->removeFromParent();
        lineNode = nullptr;
        if (towerNode) towerNode->removeFromParent();
        towerNode = nullptr;
        lastFrac = -1.f;
    }

    // CCDrawNode mezcla con alfa PREMULTIPLICADO: si no multiplicamos el color por el alfa,
    // un alfa de 0 sigue viendose. Este helper lo hace bien.
    inline ccColor4F pm(float r, float g, float b, float a) {
        a = std::clamp(a, 0.f, 1.f);
        return {r * a, g * a, b * a, a};
    }

    // Carril estilo Guitar Hero: las flechas bajan y hay que clickear cuando tocan la linea
    // marcada por los chevrones ">  <". Las barras verdes largas son los holds.
    inline void drawLane(float px) {
        if (!towerNode) return;
        towerNode->clear();

        constexpr float H = 180.f, W = 56.f;
        float warn = std::max(1.f, getWarnBlocks()) * 30.f; // 1 bloque = 30 unidades
        ccColor4F clear = {0.f, 0.f, 0.f, 0.f};

        // Fondo del carril
        CCPoint bg[4] = {{-W / 2, 0.f}, {W / 2, 0.f}, {W / 2, H}, {-W / 2, H}};
        towerNode->drawPolygon(bg, 4, pm(0.f, 0.f, 0.f, 0.38f), 1.f, pm(0.3f, 1.f, 0.4f, 0.45f));

        bool active = false;
        float nw = W - 12.f;

        for (auto const& c : loaded) {
            if (px > c.x2) continue;                 // ya paso
            if (c.x1 - px > warn) break;             // todavia muy lejos (estan ordenados)

            if (px >= c.x1) active = true;

            float yBot = std::clamp(std::max(0.f, c.x1 - px) / warn * H, 0.f, H);
            float yTop = std::clamp((c.x2 - px) / warn * H, 0.f, H);
            bool hold = (c.x2 - c.x1) > 3.f;

            // Barra del hold (cuanto mantener)
            if (hold && yTop - yBot > 0.5f) {
                CCPoint r[4] = {{-nw / 2, yBot}, {nw / 2, yBot}, {nw / 2, yTop}, {-nw / 2, yTop}};
                towerNode->drawPolygon(r, 4, pm(0.15f, 1.f, 0.25f, 0.55f), 1.f, pm(0.3f, 1.f, 0.4f, 0.95f));
            }

            // Flecha de inicio (solo mientras aun no llegas a ella)
            if (c.x1 >= px) {
                float hw = nw / 2;
                CCPoint tri[3] = {{0.f, yBot}, {-hw, yBot + 12.f}, {hw, yBot + 12.f}};
                towerNode->drawPolygon(tri, 3, pm(1.f, 1.f, 1.f, 0.95f), 1.f, pm(0.3f, 1.f, 0.4f, 1.f));
                CCPoint stem[4] = {{-hw / 2, yBot + 12.f}, {hw / 2, yBot + 12.f}, {hw / 2, yBot + 22.f}, {-hw / 2, yBot + 22.f}};
                towerNode->drawPolygon(stem, 4, pm(1.f, 1.f, 1.f, 0.95f), 1.f, pm(0.3f, 1.f, 0.4f, 1.f));
            }
        }

        // Linea de impacto: aqui debe llegar la flecha para clickear
        auto hit = active ? pm(0.3f, 1.f, 0.4f, 1.f) : pm(1.f, 1.f, 1.f, 0.9f);
        towerNode->drawSegment({-W / 2 - 6.f, 0.f}, {W / 2 + 6.f, 0.f}, active ? 3.f : 1.8f, hit);

        // Chevrones laterales ">  <" (senalan la linea de impacto)
        auto ch = pm(0.25f, 1.f, 0.35f, 0.95f);
        towerNode->drawSegment({-W / 2 - 34.f, 13.f}, {-W / 2 - 16.f, 0.f}, 3.f, ch);
        towerNode->drawSegment({-W / 2 - 16.f, 0.f}, {-W / 2 - 34.f, -13.f}, 3.f, ch);
        towerNode->drawSegment({W / 2 + 34.f, 13.f}, {W / 2 + 16.f, 0.f}, 3.f, ch);
        towerNode->drawSegment({W / 2 + 16.f, 0.f}, {W / 2 + 34.f, -13.f}, 3.f, ch);
    }

    // Crea (o recrea) todo lo que se dibuja: guias, linea del jugador y torre
    inline void refreshOverlay() {
        clearVisuals();

        auto pl = PlayLayer::get();
        if (!pl || !pl->m_objectLayer) return;
        if (!usingSaved || loaded.empty()) return;

        constexpr float bottom = -500.f;
        constexpr float top = 4000.f;
        ccColor4F clear = {0.f, 0.f, 0.f, 0.f};

        overlayRoot = CCNode::create();
        pl->m_objectLayer->addChild(overlayRoot, 1000);

        float op = std::clamp(getOpacity(), 0.f, 1.f);

        for (auto const& c : loaded) {
            auto node = CCDrawNode::create();

            // Barra verde: cuanto hay que mantener el click
            if (op > 0.01f && c.x2 - c.x1 > 0.5f) {
                CCPoint g[4] = {{c.x1, bottom}, {c.x2, bottom}, {c.x2, top}, {c.x1, top}};
                node->drawPolygon(g, 4, pm(0.15f, 1.f, 0.25f, op), 1.f, pm(0.2f, 1.f, 0.3f, op * 1.6f));
            }
            // Linea blanca: donde empezar el click
            CCPoint w[4] = {{c.x1 - 1.5f, bottom}, {c.x1 + 1.5f, bottom}, {c.x1 + 1.5f, top}, {c.x1 - 1.5f, top}};
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

        // Torre en la parte izquierda de la pantalla
        if (getShowTower() && pl->m_uiLayer) {
            auto win = CCDirector::get()->getWinSize();
            towerNode = CCDrawNode::create();
            towerNode->setPosition({win.width * 0.12f, win.height * 0.20f});
            towerNode->setScale(std::clamp(getTowerScale(), 0.4f, 2.f));
            pl->m_uiLayer->addChild(towerNode, 100);
            drawLane(pl->m_player1 ? pl->m_player1->getPositionX() : 0.f);
        }
    }

    // Se llama en cada frame: oculta guias ya pasadas, mueve la linea y actualiza la torre
    inline void updateVisuals(PlayLayer* pl) {
        if (!usingSaved || loaded.empty() || !pl || !pl->m_player1) return;
        float px = pl->m_player1->getPositionX();

        // Cada guia desaparece cuando el jugador llega al final de su barra
        size_t n = std::min(guideNodes.size(), loaded.size());
        for (size_t i = 0; i < n; i++) {
            guideNodes[i]->setVisible(px <= loaded[i].x2);
        }

        if (lineNode) lineNode->setPositionX(px);

        if (towerNode) drawLane(px);
    }

    // ---------- Operaciones ----------

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
        if (usingSaved) refreshOverlay();
        return true;
    }

    inline void deleteSelected() {
        std::error_code ec;
        if (!selectedPath.empty()) std::filesystem::remove(selectedPath, ec);
        selectedName.clear();
        selectedPath.clear();
        loaded.clear();
        usingSaved = false;
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
    }

    // ---------- Grabacion ----------

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
