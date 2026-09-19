#pragma once

#include <windows.h>

#include <string_view>

#include "vmprotect.hpp"

namespace launcher::ui {

enum class Language {
    English,
    Chinese,
    Russian,
    German,
};

struct LocalizedText {
    std::wstring_view branch;
    std::wstring_view branch_value;
    std::wstring_view target;
    std::wstring_view menu;
    std::wstring_view current_version;
    std::wstring_view note_launcher;
    std::wstring_view note_process;
    std::wstring_view note_launch;
    std::wstring_view note_feedback;
    std::wstring_view note_menu;
    std::wstring_view load;
    std::wstring_view loading;
    std::wstring_view injection_successful;
    std::wstring_view back;
    std::wstring_view about;
    std::wstring_view free_notice;
    std::wstring_view language;
};

inline Language DetectSystemLanguage() {
    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) == 0) {
        return Language::English;
    }
    if (locale[0] == L'z' && locale[1] == L'h') {
        return Language::Chinese;
    }
    if (locale[0] == L'r' && locale[1] == L'u') {
        return Language::Russian;
    }
    if (locale[0] == L'd' && locale[1] == L'e') {
        return Language::German;
    }
    return Language::English;
}

inline const LocalizedText& TextFor(Language language) {
    static constexpr LocalizedText english{
        L"Branch:", L"Release", L"Target:", L"Menu:", L"Current version",
        L"- Release version 1.0.0-beta.1", L"– Local game process detection",
        L"– Automatic game launch", L"– Real-time launch status",
        L"– Feature menu hotkey: Insert", L"Load", L"The game will be launched automatically",
        L"Injection successful", L"Back", L"About",
        L"This software is completely free.\nIf you paid for it, you were scammed.",
        L"Language"};
    static constexpr LocalizedText chinese{
        L"分支：", L"正式版", L"目标：", L"菜单：", L"当前版本",
        L"- 发布版本 1.0.0-beta.1", L"– 本地游戏进程检测", L"– 自动启动游戏", L"– 启动状态实时反馈",
        L"– 功能菜单快捷键：Insert", L"加载", L"游戏将自动启动", L"注入成功", L"返回", L"关于",
        L"此软件完全免费。\n如果你是购买的，说明你被骗了。",
        L"语言"};
    static constexpr LocalizedText russian{
        L"Ветка:", L"Релиз", L"Цель:", L"Меню:", L"Текущая версия",
        L"- Версия релиза 1.0.0-beta.1", L"– Поиск локального процесса игры",
        L"– Автоматический запуск игры", L"– Статус запуска в реальном времени",
        L"– Горячая клавиша меню: Insert", L"Загрузить", L"Игра будет запущена автоматически",
        L"Инъекция выполнена", L"Назад", L"О программе",
        L"Это программное обеспечение полностью бесплатно.\nЕсли вы заплатили за него, вас обманули.",
        L"Язык"};
    static constexpr LocalizedText german{
        L"Zweig:", L"Release", L"Ziel:", L"Menü:", L"Aktuelle Version",
        L"- Release-Version 1.0.0-beta.1", L"– Lokale Spielprozesserkennung",
        L"– Automatischer Spielstart", L"– Startstatus in Echtzeit",
        L"– Menü-Hotkey: Insert", L"Laden", L"Das Spiel wird automatisch gestartet",
        L"Injection erfolgreich", L"Zurück", L"Info",
        L"Diese Software ist vollständig kostenlos.\nWenn Sie dafür bezahlt haben, wurden Sie betrogen.",
        L"Sprache"};

    switch (language) {
    case Language::Chinese: return chinese;
    case Language::Russian: return russian;
    case Language::German: return german;
    default: return english;
    }
}

inline std::wstring_view AuthorText(Language language) {
    switch (language) {
    case Language::Chinese: {
        static const wchar_t* value = PZ_VMP_DECRYPT_STRING_W(L"作者：Initloader（呵呵）");
        return value;
    }
    case Language::Russian: {
        static const wchar_t* value = PZ_VMP_DECRYPT_STRING_W(L"Автор: Initloader（呵呵）");
        return value;
    }
    case Language::German: {
        static const wchar_t* value = PZ_VMP_DECRYPT_STRING_W(L"Autor: Initloader（呵呵）");
        return value;
    }
    default: {
        static const wchar_t* value = PZ_VMP_DECRYPT_STRING_W(L"Author: Initloader（呵呵）");
        return value;
    }
    }
}

inline std::wstring_view LanguageName(Language language) {
    switch (language) {
    case Language::Chinese: return L"中文";
    case Language::Russian: return L"Русский";
    case Language::German: return L"Deutsch";
    default: return L"English";
    }
}

}  // namespace launcher::ui
