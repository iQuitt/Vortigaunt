#include "core/VortigauntLog.h"
#include "VortigauntVersion.h"
#include "utils/Platform.h"
#include "utils/FileIO.h"
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef QT_CORE_LIB

#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QMouseEvent>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QPointer>
#endif

namespace VortigauntLog
{
    static std::string g_logFilePath;
    static std::mutex g_logMutex;

#ifdef QT_CORE_LIB
    static QList<QPointer<QPlainTextEdit>> g_logWidgets;
#endif

    
    static std::string getTimeStamp()
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "[%H:%M:%S] ");
        return ss.str();
    }

    static void writeToFile(const std::string& message)
    {
        if (g_logFilePath.empty()) return;

        // remove color codes for file log (^1, ^2, etc.)
        std::string rawMsg;
        for (size_t i = 0; i < message.length(); ++i) {
            if (message[i] == '^' && i + 1 < message.length() && isdigit(message[i+1])) {
                i++; // Skip both ^ and digit
            } else {
                rawMsg += message[i];
            }
        }

        std::ofstream file(FileIO::toPath(g_logFilePath), std::ios::app);
        if (file.is_open()) {
            file << getTimeStamp() << rawMsg << std::endl;
        }
    }

    static std::string stripColorCodes(const std::string& message)
    {
        std::string result;
        for (size_t i = 0; i < message.length(); ++i) {
            if (message[i] == '^' && i + 1 < message.length() && isdigit(message[i+1])) {
                i++; // Skip both ^ and digit
            } else {
                result += message[i];
            }
        }
        return result;
    }

#ifdef QT_CORE_LIB
    static QColor getColorForCode(QChar code, QPlainTextEdit* widget)
    {
        bool dark = true;
        if (widget) {
            dark = widget->palette().color(QPalette::Window).value() < 128; // Basic luminance heuristic
        }
        
        switch (code.toLatin1()) {
            case '0': return dark ? QColor("#E0E0E0") : QColor("#000000"); // Default
            case '1': return QColor("#FF5555"); // Red
            case '2': return dark ? QColor("#55FF55") : QColor("#00AA00"); // Green 
            case '3': return dark ? QColor("#FFFF55") : QColor("#AAAA00"); // Yellow
            case '4': return dark ? QColor("#55A0FF") : QColor("#0055AA"); // Blue
            case '5': return dark ? QColor("#55FFFF") : QColor("#00AAAA"); // Cyan
            case '6': return dark ? QColor("#FF55FF") : QColor("#AA00AA"); // Magenta
            case '7': return dark ? QColor("#E0E0E0") : QColor("#111111"); // White/Black 
            case '8': return QColor("#FF8800"); 
            case '9': return dark ? QColor("#AAAAAA") : QColor("#555555"); // Debug
            default:  return dark ? QColor("#E0E0E0") : QColor("#000000"); 
        }
    }

    static void insertFormattedLine(QPlainTextEdit* widget, const QString& message)
    {
        QTextCursor cursor = widget->textCursor();
        cursor.movePosition(QTextCursor::End);
        
        cursor.beginEditBlock();
        
        if (!widget->document()->isEmpty()) {
            cursor.insertBlock();
        }
        
        // Insert timestamp in gray
        QTextCharFormat timestampFmt;
        timestampFmt.setForeground(QColor("#888888"));
        QString timestamp = QString::fromStdString(getTimeStamp());
        cursor.insertText(timestamp, timestampFmt);
        
        // Strip color codes to get raw text for path detection
        QString rawText;
        QVector<int> rawToMsgIndex;
        {
            int i = 0;
            while (i < message.length()) {
                if (message[i] == '^' && i + 1 < message.length() && message[i + 1].isDigit()) {
                    i += 2;
                } else {
                    rawToMsgIndex.append(i);
                    rawText += message[i];
                    i++;
                }
            }
        }

        // Detect file paths in raw text
        static QRegularExpression pathRegex(
            R"(([A-Za-z]:[/\\][^\n\r"<>]*[^\s\n\r"<>]|/(?:home|tmp|usr|var|opt|mnt|media)/[^\n\r"<>]*[^\s\n\r"<>]))");

        QVector<int> pathStartsInRaw;
        QVector<int> pathEndsInRaw;
        QStringList  pathStrings;

        QRegularExpressionMatchIterator matches = pathRegex.globalMatch(rawText);
        while (matches.hasNext()) {
            QRegularExpressionMatch match = matches.next();
            pathStartsInRaw.append(match.capturedStart(1));
            pathEndsInRaw.append(match.capturedEnd(1));
            pathStrings.append(match.captured(1));
        }

        // Default text format
        QTextCharFormat defaultFmt;
        defaultFmt.setForeground(QColor("#E0E0E0"));
        
        QTextCharFormat currentFmt = defaultFmt;
        int rawIdx = 0;
        int pathIdx = 0;
        bool inLink = false;
        
        // Batch buffer — accumulate characters with the same format
        QString batchBuffer;
        QTextCharFormat batchFmt = defaultFmt;
        
        // Helper to flush the accumulated batch
        auto flushBatch = [&]() {
            if (!batchBuffer.isEmpty()) {
                cursor.insertText(batchBuffer, batchFmt);
                batchBuffer.clear();
            }
        };

        int i = 0;
        while (i < message.length()) {
            if (message[i] == '^' && i + 1 < message.length() && message[i + 1].isDigit()) {
                // Color code — flush previous batch, update format
                flushBatch();
                currentFmt = QTextCharFormat();
                currentFmt.setForeground(getColorForCode(message[i + 1], widget));
                i += 2;
            } else {
                // Check if entering a path
                bool wasInLink = inLink;
                if (!inLink && pathIdx < pathStartsInRaw.size() && rawIdx == pathStartsInRaw[pathIdx]) {
                    inLink = true;
                }

                // Build the format for this character
                QTextCharFormat fmt = currentFmt;
                if (inLink) {
                    fmt.setForeground(QColor("#4FC3F7"));
                    fmt.setFontUnderline(true);
                    QString fileUrl = QUrl::fromLocalFile(pathStrings[pathIdx]).toString();
                    fmt.setAnchor(true);
                    fmt.setAnchorHref(fileUrl);
                }

                // Check if format changed — flush if so
                if (batchFmt != fmt || wasInLink != inLink) {
                    flushBatch();
                    batchFmt = fmt;
                }
                
                batchBuffer += message[i];
                rawIdx++;

                // Check if leaving a path
                if (inLink && pathIdx < pathEndsInRaw.size() && rawIdx == pathEndsInRaw[pathIdx]) {
                    flushBatch();
                    inLink = false;
                    pathIdx++;
                }

                i++;
            }
        }
        flushBatch();
        
        cursor.endEditBlock();

        // Auto-scroll to bottom
        widget->setTextCursor(cursor);
        widget->ensureCursorVisible();
        
        widget->viewport()->update();
        widget->update();
    }
#endif


    void Initialize(const std::string& logFileName)
    {
        g_logFilePath = logFileName;
        
        // Clear log file on start
        std::ofstream file(FileIO::toPath(g_logFilePath), std::ios::trunc);
        if (file.is_open()) {
            file << "================================================================================" << std::endl;
            file << "VortigauntTool v" << VORTIGAUNT_VERSION_STRING
                 << " (" << VORTIGAUNT_BUILD_TYPE << ")" << std::endl;
            file << "Commit: " << VORTIGAUNT_GIT_COMMIT
                 << " | Branch: " << VORTIGAUNT_GIT_BRANCH << std::endl;
            file << "OS: " << VORTIGAUNT_OS
                 << " | Built: " << __DATE__ << " " << __TIME__ << std::endl;
            file << "Tool Started at: " << __DATE__ << getTimeStamp() << std::endl;
            file << "================================================================================" << std::endl;
        }
    }

#ifdef QT_CORE_LIB
    class LogLinkEventFilter : public QObject {
    public:
        LogLinkEventFilter(QObject* parent = nullptr) : QObject(parent) {}
        
        bool eventFilter(QObject* obj, QEvent* event) override {
            if (event->type() == QEvent::MouseButtonRelease) {
                auto* edit = qobject_cast<QPlainTextEdit*>(obj->parent());
                auto* me = static_cast<QMouseEvent*>(event);
                if (edit && me->button() == Qt::LeftButton) {
                    QString anchor = edit->anchorAt(me->pos());
                    if (!anchor.isEmpty()) {
                        QDesktopServices::openUrl(QUrl(anchor));
                        return true;
                    }
                }
            } else if (event->type() == QEvent::MouseMove) {
                auto* edit = qobject_cast<QPlainTextEdit*>(obj->parent());
                auto* me = static_cast<QMouseEvent*>(event);
                if (edit) {
                    QString anchor = edit->anchorAt(me->pos());
                    if (!anchor.isEmpty()) {
                        edit->viewport()->setCursor(Qt::PointingHandCursor);
                    } else {
                        edit->viewport()->setCursor(Qt::IBeamCursor);
                    }
                }
            }
            return QObject::eventFilter(obj, event);
        }
    };
    
    static LogLinkEventFilter* g_linkHandler = nullptr;

    void addLogWidget(QPlainTextEdit* widget) {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (!widget) return;
        
        // Don't add duplicates
        if (!g_logWidgets.contains(widget)) {
            g_logWidgets.append(widget);
            
            if (!g_linkHandler) {
                g_linkHandler = new LogLinkEventFilter();
            }
            widget->viewport()->installEventFilter(g_linkHandler);
            widget->setMouseTracking(true);
        }
    }
#endif



    bool isDeveloperMode() {
#ifdef QT_CORE_LIB
        QSettings settings("Vortigaunt", "VortigauntTool");
        return settings.value("General/DeveloperMode", false).toBool();
#else
        return false;
#endif
    }

    void setDeveloperMode(bool enabled) {
#ifdef QT_CORE_LIB
        QSettings settings("Vortigaunt", "VortigauntTool");
        settings.setValue("General/DeveloperMode", enabled);
#endif
    }
    
    // Helper: strip leading whitespace and color codes to find the log prefix
    static std::string stripLeadingForPrefixCheck(const std::string& msg)
    {
        size_t pos = 0;
        // Skip leading whitespace
        while (pos < msg.size() && (msg[pos] == ' ' || msg[pos] == '\t'))
            pos++;
        // Skip leading color codes (^0 .. ^9)
        while (pos + 1 < msg.size() && msg[pos] == '^' && isdigit(msg[pos + 1]))
            pos += 2;
        return msg.substr(pos);
    }

    // Core internal logger - auto-detects log level from message prefix
#ifdef QT_CORE_LIB
    void LogInternal(const std::string& msg, bool debugOnly, QPlainTextEdit* targetWidget)
#else
    void LogInternal(const std::string& msg, bool debugOnly)
#endif
    {
        std::string processedMsg = msg;
        bool isDebug = debugOnly;

        std::string check = stripLeadingForPrefixCheck(msg);

        if (check.rfind("ERROR:", 0) == 0 || check.rfind("[ERROR]", 0) == 0) {
            processedMsg = "^1" + msg;  // Red
        } else if (check.rfind("WARNING:", 0) == 0 || check.rfind("[WARNING]", 0) == 0) {
            processedMsg = "^3" + msg;  // Yellow
        } else if (check.rfind("DEBUG:", 0) == 0) {
            processedMsg = "^9" + msg;  // Gray
            isDebug = true;
        }

        if (isDebug && !isDeveloperMode()) return;

        std::lock_guard<std::mutex> lock(g_logMutex);

        writeToFile(processedMsg);


#ifdef QT_CORE_LIB
        // Clean up invalid pointers
        g_logWidgets.removeAll(nullptr);

        for (auto& logWidget : g_logWidgets) {
            QPlainTextEdit* destWidget = targetWidget ? targetWidget : logWidget.data();
            if (destWidget) {
                QString qMsg = QString::fromStdString(processedMsg);
                QMetaObject::invokeMethod(destWidget, [destWidget, qMsg](){
                    if (destWidget) {
                        insertFormattedLine(destWidget, qMsg);
                    }
                }, Qt::QueuedConnection);
            }
        }
#endif
    }

    void Vortigaunt_Printf(const std::string& message) {
        LogInternal(message, false);
    }

    void Vortigaunt_Printf(const char* message) {
        LogInternal(std::string(message), false);
    }

#ifdef QT_CORE_LIB
    void Vortigaunt_Printf(const QString& message) {
        LogInternal(message.toStdString(), false);
    }

    void Vortigaunt_Printf(QPlainTextEdit* logWidget, const QString& message) {
        LogInternal(message.toStdString(), false, logWidget);
    }
#endif

}
