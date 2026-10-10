// src/App.h
#pragma once

#include <windows.h>

#include <string>
#include <vector>
#include <memory>
#include <array>

#include "imgui.h"
#include "TextEditor.h"
#include "core/FileSystem.h"

namespace mayami {

    struct EditorTab {
        std::string title;
        std::string path;
        std::string savedContent;
        bool modified = false;
        TextEditor editor;
    };

    enum class Theme { Dark, Light };

    class App {
    public:
        App();
        ~App();

        int run();

    private:
        bool initWin32();
        bool initImGui();
        void mainLoop();
        void shutdown();

        void drawUI();
        void drawTitleBar();
        void drawMenuBar();
        void drawFileTree();
        void drawEditor();
        void drawStatusBar();
        void drawTerminalWindow();
        void drawAboutModal();
        void drawNewFileDialog();
        void drawNewFolderDialog();

        void applyTheme();
        void applyFonts();

        void newFile();
        void openFile();
        void openProject();
        void saveFile();
        void openFileInEditor(const std::string& path);

        void runCode();
        void buildExe();
        void runExe();

        void log(const std::string& msg);
        void logError(const std::string& msg);
        void logSuccess(const std::string& msg);
        void logWarn(const std::string& msg);
        void logOutput(const std::string& msg);
        void logOutputError(const std::string& msg);
        void clearBuildLog();
        void clearOutputLog();

        HWND m_hwnd = nullptr;
        bool m_shutdownCalled = false;

        float m_titleBarHeight = 34.0f;
        float m_menuBarHeight = 30.0f;
        float m_statusBarHeight = 24.0f;
        float m_sidePanelWidth = 260.0f;

        Theme m_theme = Theme::Dark;

        ImFont* m_fontText = nullptr;
        ImFont* m_fontMono = nullptr;
        ImFont* m_fontIcon = nullptr;

        std::vector<EditorTab> m_tabs;
        int m_activeTab = -1;

        // ★ Дебаг
        bool m_debugMode = false;

        std::string m_lastBuiltExe;

        std::string m_projectRoot;
        FileEntry   m_fileTreeRoot;
        std::string m_selectedPath;

        std::vector<std::string> m_buildLog;
        std::vector<std::string> m_outputLog;

        bool m_autoScrollBuild = true;
        bool m_autoScrollOutput = true;
        int  m_terminalTab = 0;

        bool m_showTerminalWindow = false;
        bool m_showAbout = false;
        bool m_showNewFileDialog = false;
        bool m_showNewFolderDialog = false;

        char m_newFileName[256] = {};
        char m_newFolderName[256] = {};
    };

} // namespace mayami