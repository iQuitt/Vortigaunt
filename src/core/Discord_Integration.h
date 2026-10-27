// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once
#include <memory>
#include <string>

namespace discordpp { class Client; }

class Discord_Integration
{
public:
    Discord_Integration();
    ~Discord_Integration();

    void Initialize();
    void UpdatePresence();
    void ClearPresence();
    void Shutdown();
    void RunCallbacks();

private:
    void StartAuthorization();
    void RefreshSavedToken();

    std::unique_ptr<discordpp::Client> m_client;
    std::string m_codeVerifierStr;
    std::string m_refreshToken;
    bool m_connected = false;
};