#include <Geode/Geode.hpp>
#include <Geode/modify/CreateGuidelinesLayer.hpp>
#include "osu_parser.hpp"

using namespace geode::prelude;

std::string m_pendingGuidelineString;
bool m_hasImportedData = false;
double m_offset = 0.0;

class $modify(MyCreateGuidelinesLayer, CreateGuidelinesLayer) {
    bool init(CustomSongDelegate* p0, AudioGuidelinesType p1) {
        if (!CreateGuidelinesLayer::init(p0, p1)) {
            return false;
        }

        createImportButton();
        createOffsetInput();
        return true;
    }

    void update(float dt) {
        CreateGuidelinesLayer::update(dt);
        
        if (m_hasImportedData) {
            m_hasImportedData = false;
            m_guidelineString = std::move(m_pendingGuidelineString);
            onStop(nullptr);
            FLAlertLayer::create("Success", "Map was successfully imported!", "OK")->show();
        }
    }

    void createImportButton() {
        auto buttonImport = ButtonSprite::create(
            "Import .osu", 120, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.7f
        );
        
        auto buttonImportClick = CCMenuItemExt::createSpriteExtra(
            buttonImport, 
            [this](CCMenuItemSpriteExtra*) { handleImportClick(); }
        );

        buttonImportClick->setPosition({145, -95});
        m_buttonMenu->addChild(buttonImportClick);
    }

    void createOffsetInput() {
        auto input = TextInput::create(80.f, "MS Offset (-0.0)", "chatFont.fnt");
        if (m_offset != 0.0)
            input->setString(fmt::format("{}", m_offset));
        input->setFilter("1234567890.-");
        input->setCallback([](const std::string& text) {
            auto msOffsetResult = geode::utils::numFromString<double>(text);
            m_offset = msOffsetResult.isErr() ? 0.f : msOffsetResult.unwrap();
        });
        input->setPosition({145.f, -60.f});
        m_buttonMenu->addChild(input);
    }
	
    void handleImportClick() {
        static const geode::utils::file::FilePickOptions::Filter OSU_FILTER = {
            .description = ".osu (file format)",
            .files = {"*.osu"}
        };

        geode::utils::file::pick(
            geode::utils::file::PickMode::OpenFile, 
            {std::nullopt, {OSU_FILTER}}
        ).listen([this](geode::Result<std::filesystem::path>* result) {
            if (!result->isErr()) {
                processOsuFile(result->unwrap());
            } else {
                showErrorAlert();
            }
        });
    }

    void processOsuFile(const std::filesystem::path& path) {
        OsuParser parser;
        
        if (parser.parseFile(path)) {
            m_pendingGuidelineString = parser.generateGuidelinesString(m_offset);
            m_hasImportedData = true;
            onRecord(nullptr);
        } else {
            showErrorAlert();
        }
    }

    void showErrorAlert() {
        FLAlertLayer::create("Error", "Failed to parse .osu file!", "OK")->show();
    }
};