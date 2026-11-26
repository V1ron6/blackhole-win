#pragma once
#include <string>
#include <vector>

// Stage 6: shared loader for the three built-in wordlists (common_paths.txt,
// common_passwords.txt, common_usernames.txt - all copied from the
// original app's res/raw/ into assets/) and for parsing a pasted custom
// wordlist textbox. Used by DirectoryBusterDialog, HashCrackerDialog, and
// HydraLiteDialog.
std::vector<std::wstring> LoadWordlistFile(const std::wstring& path);
std::vector<std::wstring> ParseWordlistText(const std::wstring& text);

// Folder the running .exe lives in (same computation BlackholeApp::Initialize
// does for exeDir_) - used to build paths like "<exeDir>\assets\common_paths.txt".
std::wstring ExeDirectory();
