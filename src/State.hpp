#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
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

    // ---------- Overlay (linea blanca + barra verde) ----------

    inline void refreshOverlay() {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_objectLayer) return;

        if (auto old = pl->m_objectLayer->getChildByID("overlay"_spr)) old->removeFromParent();
        if (!usingSaved || loaded.empty()) return;

        constexpr float bottom = -500.f;
        constexpr float top = 4000.f;

        auto node = CCDrawNode::create();
        node->setID("overlay"_spr);

        for (auto const& c : loaded) {
            // Barra verde: cuanto hay que mantener el click
            if (c.x2 - c.x1 > 0.5f) {
                CCPoint g[4] = {{c.x1, bottom}, {c.x2, bottom}, {c.x2, top}, {c.x1, top}};
                node->drawPolygon(g, 4, ccc4f(0.15f, 1.f, 0.25f, 0.30f), 1.f, ccc4f(0.2f, 1.f, 0.3f, 0.75f));
            }
            // Linea blanca: donde empezar el click
            CCPoint w[4] = {{c.x1 - 1.5f, bottom}, {c.x1 + 1.5f, bottom}, {c.x1 + 1.5f, top}, {c.x1 - 1.5f, top}};
            node->drawPolygon(w, 4, ccc4f(1.f, 1.f, 1.f, 0.95f), 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
        }

        pl->m_objectLayer->addChild(node, 1000);
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
