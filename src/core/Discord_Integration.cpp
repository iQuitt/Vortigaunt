#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"

#include "Discord_Integration.h"
#include "core/VortigauntVersion.h"
#include "core/VortigauntLog.h"
#include <cstdio>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <cstdlib>

using namespace VortigauntLog;

static const uint64_t APP_ID = 1469507837167272189;
static const char* REDIRECT_URI = "http://127.0.0.1/callback";

static int64_t epochtime = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

static std::string GetTokenFilePath()
{
    std::filesystem::path dir;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    dir = appData ? std::filesystem::path(appData) / "VortigauntTool" : std::filesystem::path(".");
#else
    const char* home = std::getenv("HOME");
    dir = home ? std::filesystem::path(home) / ".local" / "share" / "Vortigaunt" : std::filesystem::path(".");
#endif
    std::filesystem::create_directories(dir);
    return (dir / "discord_token.dat").string();
}

static bool SaveTokens(const std::string& accessToken, const std::string& refreshToken)
{
    std::ofstream file(GetTokenFilePath());
    if (!file.is_open()) return false;
    file << accessToken << "\n" << refreshToken << "\n";
    return true;
}

static bool LoadTokens(std::string& accessToken, std::string& refreshToken)
{
    std::ifstream file(GetTokenFilePath());
    if (!file.is_open()) return false;
    if (!std::getline(file, accessToken) || accessToken.empty()) return false;
    if (!std::getline(file, refreshToken)) refreshToken.clear();
    return true;
}

Discord_Integration::Discord_Integration() = default;
Discord_Integration::~Discord_Integration() { Shutdown(); }

void Discord_Integration::Initialize()
{
    m_client = std::make_unique<discordpp::Client>();
    m_client->SetApplicationId(APP_ID);

    m_client->SetStatusChangedCallback(
        [this](discordpp::Client::Status status,
               discordpp::Client::Error error,
               int32_t errorDetail) {
            if (status == discordpp::Client::Status::Ready) {
                m_connected = true;
                UpdatePresence();
            }
            else if (status == discordpp::Client::Status::Disconnected) {
                LogF("Discord: Disconnected (error=%d, detail=%d)",
                     static_cast<int>(error), errorDetail);
                m_connected = false;

                if (errorDetail == 4004 && !m_refreshToken.empty()) {
                    RefreshSavedToken();
                }
            }
        });

    // Try saved token first, otherwise start fresh authorization
    std::string savedAccess, savedRefresh;
    if (LoadTokens(savedAccess, savedRefresh)) {
        m_refreshToken = savedRefresh;
        m_client->UpdateToken(discordpp::AuthorizationTokenType::Bearer, savedAccess,
            [this](discordpp::ClientResult result) {
                if (result.Successful()) {
                    m_client->Connect();
                }
                else {
                    StartAuthorization();
                }
            });
    }
    else {
        StartAuthorization();
    }
}

void Discord_Integration::StartAuthorization()
{
    if (!m_client) return;

    auto codeVerifier = m_client->CreateAuthorizationCodeVerifier();
    m_codeVerifierStr = codeVerifier.Verifier();

    discordpp::AuthorizationArgs args;
    args.SetClientId(APP_ID);
    args.SetScopes(discordpp::Client::GetDefaultPresenceScopes());
    args.SetCodeChallenge(codeVerifier.Challenge());

    m_client->Authorize(std::move(args),
        [this](discordpp::ClientResult result,
               std::string code,
               std::string redirectUri) {
            if (!result.Successful()) {
                LogF("Discord: Authorization failed: %s", result.Error().c_str());
                return;
            }

            m_client->GetToken(APP_ID, code, m_codeVerifierStr, REDIRECT_URI,
                [this](discordpp::ClientResult tokenResult,
                   std::string accessToken,
                   std::string refreshToken,
                   discordpp::AuthorizationTokenType tokenType,
                   int32_t expiresIn,
                   std::string scopes) {
                    if (!tokenResult.Successful()) {
                        LogF("Discord: Token exchange failed: %s", tokenResult.Error().c_str());
                        return;
                    }

                    m_refreshToken = refreshToken;
                    SaveTokens(accessToken, refreshToken);

                    m_client->UpdateToken(tokenType, accessToken,
                        [this](discordpp::ClientResult updateResult) {
                            if (updateResult.Successful()) {
                                m_client->Connect();
                            }
                        });
                });
        });
}

void Discord_Integration::RefreshSavedToken()
{
    if (!m_client || m_refreshToken.empty()) return;

    m_client->RefreshToken(APP_ID, m_refreshToken,
        [this](discordpp::ClientResult result,
               std::string accessToken,
               std::string refreshToken,
               discordpp::AuthorizationTokenType tokenType,
               int32_t expiresIn,
               std::string scopes) {
            if (!result.Successful()) {
                StartAuthorization();
                return;
            }

            m_refreshToken = refreshToken;
            SaveTokens(accessToken, refreshToken);

            m_client->UpdateToken(tokenType, accessToken,
                [this](discordpp::ClientResult updateResult) {
                    if (updateResult.Successful()) {
                        m_client->Connect();
                    }
                });
        });
}

void Discord_Integration::UpdatePresence()
{
    if (!m_client || !m_connected) return;

    discordpp::Activity activity;
    activity.SetType(discordpp::ActivityTypes::Playing);
    activity.SetName("Vortigaunt");
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Version %s.beta", VORTIGAUNT_VERSION_STRING);
    activity.SetState(std::string(buffer));
    activity.SetDetails(std::string("A Porting tool For Goldsrc Engine"));

    discordpp::ActivityTimestamps timestamps;
    timestamps.SetStart(static_cast<uint64_t>(epochtime));
    activity.SetTimestamps(timestamps);

    discordpp::ActivityAssets assets;
    assets.SetLargeImage(std::string("vortiicon"));

    assets.SetLargeText(std::string(buffer));
    activity.SetAssets(assets);

    discordpp::ActivityButton btn1;
    btn1.SetLabel("GitHub");
    btn1.SetUrl("https://github.com/iQuitt/Vortigaunt");
    activity.AddButton(std::move(btn1));

    discordpp::ActivityButton btn2;
    btn2.SetLabel("Discord Server");
    btn2.SetUrl("https://discord.gg/PZ9JzgHHKa");
    activity.AddButton(std::move(btn2));

    m_client->UpdateRichPresence(std::move(activity),
        [](discordpp::ClientResult result) {
            if (result.Successful()) {
                Vortigaunt_Printf("Connected to Discord!");
            }
            else {
                Vortigaunt_Printf("Failed to connect to Discord!");
            }
        });
}

void Discord_Integration::ClearPresence()
{
    if (m_client && m_connected) {
        m_client->ClearRichPresence();
    }
}

void Discord_Integration::RunCallbacks()
{
    discordpp::RunCallbacks();
}

void Discord_Integration::Shutdown()
{
    if (m_client) {
        ClearPresence();
        m_client->Disconnect();
        m_client.reset();
        m_connected = false;
    }
}
