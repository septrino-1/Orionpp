#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"

// Qt 核心模块
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFontDialog>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QTreeView>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QDialog>

// 网络相关
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QSplitter>

// 界面组件
#include <qlabel.h>

// 平台特定功能
#ifdef Q_OS_WIN
#include <windows.h>
#endif

// ==================== 构造函数和初始化 ====================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::mainWindow)
{
    ui->setupUi(this);

    // 初始化网络管理器
    manager = new QNetworkAccessManager(this);

    // -------------------- 信号槽连接 --------------------
    setupConnections();

    // -------------------- 界面样式设置 --------------------
    setupUI();
    setupWelcomeTab();

    // -------------------- 输出窗口初始化 --------------------
    setupOutputWindow();

    // -------------------- 项目树初始化 --------------------
    setupProjectTree();

    // -------------------- 状态栏初始化 --------------------
    setupStatusBar();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ==================== 初始化函数 ====================
void MainWindow::setupConnections()
{
    // 文件操作
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::newFileInProject);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::exitApp);

    // 编辑操作
    connect(ui->actionFont, &QAction::triggered, this, &MainWindow::setFont);
    connect(ui->actionColor, &QAction::triggered, this, &MainWindow::setColor);
    connect(ui->actionFindText, &QAction::triggered, this, &MainWindow::findText);
    connect(ui->actionFindNext, &QAction::triggered, this, &MainWindow::findNext);
    connect(ui->actionFindPrevious, &QAction::triggered, this, &MainWindow::findPrevious);

    // 编译运行
    connect(ui->actionCompile, &QAction::triggered, this, &MainWindow::compileCurrentFile);
    connect(ui->actionRun, &QAction::triggered, this, &MainWindow::runCurrentFile);

    // AI功能
    connect(ui->actionAIImprove, &QAction::triggered, this, &MainWindow::aiImproveCode);
    connect(ui->aiChatInput, &QPlainTextEdit::textChanged, this, &MainWindow::checkEnterPressed);
    connect(ui->btnClearHistory, &QPushButton::clicked, this, &MainWindow::clearConversationHistory);

    //help
    connect(ui->actionHelp, &QAction::triggered, this, &MainWindow::showHelp);

    // 项目操作
    connect(ui->actionOpenProject, &QAction::triggered, this, [=]() {
        chooseProjectDirectory("");
    });
    connect(ui->actionNewProject, &QAction::triggered, this, &MainWindow::createProject);

    // 标签页操作
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [=](int index) {
        QWidget* tab = ui->tabWidget->widget(index);
        if (tabFilePaths.contains(tab))
            currentFilePath = tabFilePaths.value(tab);
        else
            currentFilePath = "";
    });

    // 标签页右键菜单
    ui->tabWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tabWidget, &QTabWidget::customContextMenuRequested,
            this, &MainWindow::showTabContextMenu);
}

void MainWindow::setupUI()
{
    // 设置主窗口样式
    this->setStyleSheet(
        "QMainWindow::separator {"
        "background-color: #b0b8c8;"   // 和工具栏背景低饱和蓝灰一致
        "width: 4px;"                  // 垂直分割线
        "height: 4px;"                 // 水平分割线
        "}"
        );

    // 设置AI聊天输出区域
    ui->aiChatOutput->setPlaceholderText(
        "✨ 欢迎使用 CIDE AI 助手 ✨\n👋 你好！这里是 DeepSeek AI（已接入 DeepSeek-V3.1 Reasoner）\n🚀 它将成为你最懂的 C/C++ 开发伙伴\n\n💡 使用方法：\n📝 输入 C/C++ 代码 → AI 会帮你优化、补全和改进\n🔍 提出问题 → AI 会耐心解释并给出示例\n🛠️ 调试错误 → AI 会分析问题并给出解决方案\n🎨 优化风格 → AI 可美化你的代码结构\n\n🌈 现在就试试吧 —— 输入你的问题或代码片段开始体验！\n"
        );

    ui->aiChatOutput->setStyleSheet(R"(
    QTextEdit {
        font-family: 'Simsun';
        font-size: 12pt;        /* 调大字体 */
        color: #333333;         /* 输入文字颜色 */
    }
    QTextEdit:empty {
        color: #999999;         /* placeholder 颜色 */
        font-style: italic;     /* placeholder 斜体 */
    }
)");
}

void MainWindow::setupOutputWindow()
{
    // 设置输出窗口字体
    QFont font("JetBrains Mono, SimHei", 12);
    font.setStyleStrategy(QFont::PreferAntialias);
    ui->outputWindow->setFont(font);

    // 设置输出窗口样式
    ui->outputWindow->setStyleSheet(R"(
    QPlainTextEdit {
        background: #ffffff;
        color: #2d2d2d;
        border: 1px solid #cccccc;
        border-radius: 8px;
        padding: 12px;
        selection-background-color: #cce5ff;
        selection-color: #000000;
    }
)");

    // 设置输出窗口初始文本
    ui->outputWindow->setPlainText(
        "💡 编译输出窗口已初始化完成！此处将显示程序的编译信息、警告和错误。\n"
        "✨---------------------------------------------------------------✨\n"
        );
}

void MainWindow::setupProjectTree()
{
    ui->projectTree->setModel(nullptr);
    projectModel = nullptr;
    currentProjectPath = "";
}

void MainWindow::setupWelcomeTab()
{
    // 创建欢迎页面容器
    QWidget *welcomeTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(welcomeTab);
    layout->setAlignment(Qt::AlignCenter);

    // 添加应用图标或logo
    QLabel *logoLabel = new QLabel();
    QPixmap logoPixmap(":/new/prefix1/images/logo2.png"); // 使用资源文件中的图片
    if (logoPixmap.isNull()) {
        // 如果没有图片资源，使用文字替代
        logoLabel->setText("C++ IDE");
        logoLabel->setStyleSheet("font-size: 4px; font-weight: bold; color: #2c3e50;");
    } else {
        logoLabel->setPixmap(logoPixmap.scaled(650, 650, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    logoLabel->setAlignment(Qt::AlignCenter);

    // 添加欢迎文字
    QLabel *welcomeText = new QLabel();
    welcomeText->setText(
        "<div style='text-align:center; width:500px; margin:0 auto; line-height:1.7; font-family:宋体, SimSun, serif;'>"
        "<h1 style='margin-bottom:0px; font-size:24px; font-weight:bold;'>欢迎使用 C/C++ IDE</h1>"
        "<p style='font-size:20px; margin-bottom:30px;'>一款简洁高效的 C/C++ 集成开发环境，为您提供流畅的编程体验</p>"

        "<p style='font-size:16px; margin-bottom:12px; font-weight:bold;'>核心功能：</p>"
        "<ul style='text-align:left; margin:0 auto 20px auto; display:inline-block; padding-left:20px; font-size:15px;'>"
        "<li style='margin-bottom:8px;'>创建和管理 C/C++ 项目，支持从零开始或导入现有项目</li>"
        "<li style='margin-bottom:8px;'>高效源代码编辑，提供智能缩进和语法高亮</li>"
        "<li style='margin-bottom:8px;'>一键编译运行，简化开发流程</li>"
        "<li style='margin-bottom:8px;'>AI 助手支持，提供代码补全和优化建议</li>"
        "</ul>"

        "<p style='font-size:18px; margin-top:20px; color:#666666;'>从菜单开始，开启您的开发之旅</p>"
        "</div>"
        );
    welcomeText->setAlignment(Qt::AlignCenter);
    welcomeText->setWordWrap(true);
    welcomeText->setStyleSheet("font-size: 14px; color: #34495e;");

    // 添加到布局
    layout->addWidget(logoLabel);
    layout->addSpacing(20);
    layout->addWidget(welcomeText);

    // 添加到标签页
    int tabIndex = ui->tabWidget->addTab(welcomeTab, "Start Page");
    ui->tabWidget->setCurrentIndex(tabIndex);

    // 设置标签页不可关闭
    ui->tabWidget->tabBar()->setTabButton(tabIndex, QTabBar::RightSide, nullptr);

    // 记录欢迎页面
    welcomeTabPage = welcomeTab;
}


void MainWindow::setupStatusBar()
{
    // 创建状态栏控件
    QLabel *cursorPosLabel = new QLabel(this);   // 显示行列
    QLabel *statusLabel = new QLabel(this);


    // 默认文本
    cursorPosLabel->setText("Line: 1, Col: 1");
    statusLabel->setText("Saved");
    // 添加到状态栏
    statusBar()->addPermanentWidget(cursorPosLabel);
    statusBar()->addPermanentWidget(statusLabel);  // ✅ 放到右边
    statusBar()->showMessage("Ready");

    // --- 光标位置更新 ---
    auto updateCursorPos = [this, cursorPosLabel]() {
        if (CodeEditor* editor = currentEditor()) {
            QTextCursor cursor = editor->textCursor();
            int line = cursor.blockNumber() + 1;      // 行号从1开始
            int col  = cursor.positionInBlock() + 1;  // 列号从1开始
            cursorPosLabel->setText(
                QString("Line: %1, Col: %2").arg(line).arg(col));
        }
    };

    // --- 修改状态更新 ---
    auto updateModifiedStatus = [this, statusLabel](bool modified) {
        statusLabel->setText(modified ? "Modified" : "Saved");
    };

    // --- 每次切换 Tab 时，重新绑定信号 ---
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [=](int){
        CodeEditor* editor = currentEditor();
        if (!editor) return;

        // 先断开旧连接（避免重复绑定）
        disconnect(editor, nullptr, cursorPosLabel, nullptr);
        disconnect(editor, nullptr, statusLabel, nullptr);

        // 光标更新
        connect(editor, &CodeEditor::cursorPositionChanged, this, updateCursorPos);

        // 文件修改状态更新
        connect(editor->document(), &QTextDocument::modificationChanged,
                this, updateModifiedStatus);

        // 初始状态
        updateCursorPos();
        updateModifiedStatus(editor->document()->isModified());
    });



}

//用户手册 帮助
void MainWindow::showHelp() {
    QDialog* helpDialog = new QDialog(this);
    helpDialog->setWindowTitle("CIDE 用户手册");
    helpDialog->resize(850, 650);
    helpDialog->setStyleSheet("QDialog { background-color: #f8f9fa; }");

    QTextBrowser* textBrowser = new QTextBrowser(helpDialog);
    textBrowser->setOpenExternalLinks(true);
    textBrowser->setStyleSheet(R"(
        QTextBrowser {
            background-color: white;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            padding: 20px;
            font-family: 'SimSun', '宋体', serif;
            font-size: 14px;
            line-height: 1.6;
        }
    )");

    textBrowser->setHtml(R"(
        <div style="font-family: 'SimSun', '宋体', serif; font-size: 15px; line-height: 1.7; color: #333;">
        <h1 style="color: #2c3e50; text-align: center; font-size: 24px; margin-bottom: 25px;">CIDE 集成开发环境用户手册</h1>

        <h2 style="color: #3498db; font-size: 20px; border-bottom: 2px solid #3498db; padding-bottom: 5px;">📁 文件操作</h2>
        <ul style="margin-top: 10px; margin-bottom: 20px;">
            <li style="margin-bottom: 8px;"><b>新建文件/项目</b> - 通过菜单栏或快捷键创建新的源代码文件或完整项目</li>
            <li style="margin-bottom: 8px;"><b>打开文件/项目</b> - 打开现有的C/C++文件或整个项目目录</li>
            <li style="margin-bottom: 8px;"><b>保存文件</b> - 保存当前编辑的文件，支持保存和另存为功能</li>
            <li style="margin-bottom: 8px;"><b>文件标签页</b> - 支持多文件同时编辑，标签页显示文件名和修改状态</li>
        </ul>

        <h2 style="color: #3498db; font-size: 20px; border-bottom: 2px solid #3498db; padding-bottom: 5px;">✏️ 编辑功能</h2>
        <ul style="margin-top: 10px; margin-bottom: 20px;">
            <li style="margin-bottom: 8px;"><b>代码编辑器</b> - 支持C/C++语法高亮、自动缩进和代码折叠</li>
            <li style="margin-bottom: 8px;"><b>字体和颜色设置</b> - 可自定义编辑器字体和文本颜色</li>
            <li style="margin-bottom: 8px;"><b>查找和替换</b> - 支持文本查找、高亮显示和导航功能</li>
            <li style="margin-bottom: 8px;"><b>光标位置显示</b> - 状态栏实时显示当前行号和列号</li>
        </ul>

        <h2 style="color: #3498db; font-size: 20px; border-bottom: 2px solid #3498db; padding-bottom: 5px;">🔄 编译与运行</h2>
        <ul style="margin-top: 10px; margin-bottom: 20px;">
            <li style="margin-bottom: 8px;"><b>编译当前文件</b> - 使用内置的GCC编译器编译当前打开的源文件</li>
            <li style="margin-bottom: 8px;"><b>运行程序</b> - 执行编译后的程序，Windows平台会在命令提示符中运行</li>
            <li style="margin-bottom: 8px;"><b>输出窗口</b> - 显示编译过程的详细输出、错误和警告信息</li>
            <li style="margin-bottom: 8px;"><b>项目管理</b> - 支持多文件项目的编译，自动收集项目中的所有源文件</li>
        </ul>

        <h2 style="color: #3498db; font-size: 20px; border-bottom: 2px solid #3498db; padding-bottom: 5px;">🤖 AI 辅助编程</h2>
        <ul style="margin-top: 10px; margin-bottom: 20px;">
            <li style="margin-bottom: 8px;"><b>AI代码改进</b> - 使用DeepSeek AI分析并改进当前代码</li>
            <li style="margin-bottom: 8px;"><b>AI对话助手</b> - 在聊天界面中与AI交流编程问题</li>
            <li style="margin-bottom: 8px;"><b>Markdown支持</b> - AI回复支持Markdown格式，包括代码块高亮</li>
            <li style="margin-bottom: 8px;"><b>对话历史</b> - 对话根据多轮上下文，支持清空历史记录</li>
        </ul>

        <h2 style="color: #3498db; font-size: 20px; border-bottom: 2px solid #3498db; padding-bottom: 5px;">🌳 项目管理</h2>
        <ul style="margin-top: 10px; margin-bottom: 20px;">
            <li style="margin-bottom: 8px;"><b>项目树视图</b> - 侧边栏显示项目文件结构，支持文件双击打开</li>
            <li style="margin-bottom: 8px;"><b>文件重命名</b> - 通过标签页右键菜单重命名文件</li>
            <li style="margin-bottom: 8px;"><b>新建项目文件</b> - 在项目中创建指定类型的源文件</li>
        </ul>

        <h2 style="color: #3498db; font-size: 20px; border-bottom: 2px solid #3498db; padding-bottom: 5px;">🎨 界面定制</h2>
        <ul style="margin-top: 10px; margin-bottom: 20px;">
            <li style="margin-bottom: 8px;"><b>主题样式</b> - 应用内置的现代化界面风格</li>
            <li style="margin-bottom: 8px;"><b>布局管理</b> - 可调整的窗口分割和停靠区域</li>
            <li style="margin-bottom: 8px;"><b>状态信息</b> - 底部状态栏显示文件状态和编辑器信息</li>
        </ul>

        <h2 style="color: #3498db; font-size: 20px; border-bottom: 2px solid #3498db; padding-bottom: 5px;">💡 使用技巧</h2>
        <ul style="margin-top: 10px; margin-bottom: 20px;">
            <li style="margin-bottom: 8px;">使用<code style="background: #f0f0f0; padding: 2px 5px; border-radius: 3px;">Enter</code>在AI聊天框中快速发送消息</li>
            <li style="margin-bottom: 8px;">项目中的文件修改后会显示星号(*)标记，保存后消失</li>
            <li style="margin-bottom: 8px;">输出窗口会显示编译耗时和生成的可执行文件路径</li>
            <li style="margin-bottom: 8px;">AI代码改进功能会保留原始代码结构并添加改进建议注释</li>
        </ul>

        <hr style="border: 0; border-top: 1px solid #ddd; margin: 30px 0;">
        <p style="text-align: center; color: #7f8c8d; font-size: 14px;">
            CIDE - C/C++集成开发环境 | 版本 1.0<br>
            如有问题或建议，请联系开发团队：teliphone11i6@gmail.com或https://github.com/septrino-1/OpenCIDE_Orionpp
        </p>
        </div>
    )");

    QVBoxLayout* layout = new QVBoxLayout(helpDialog);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->addWidget(textBrowser);
    helpDialog->setLayout(layout);

    helpDialog->exec();
}
// ==================== AI相关功能 ====================

void MainWindow::sendToAI(const QString &userText)
{
    // ---------- 显示用户输入 ----------
    QTextCursor cursor(ui->aiChatOutput->textCursor());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("你: " + userText + "\n");
    ui->aiChatOutput->setTextCursor(cursor);

    // ---------- 构建请求 ----------
    QNetworkRequest request(QUrl("https://api.deepseek.com/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString apiKey = "sk-3290f32686b7419f8422491021d4c317"; // 你的 Key
    request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

    QJsonObject body;
    body["model"] = "deepseek-reasoner";

    // 系统提示词
    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content",
         "你是一个资深的C/C++开发助手、代码优化专家和教育型助理。\n"
         "规则：\n"
         "1. 可以输出 Markdown（代码块、列表、标题）。\n"
         "2. 代码请放在 ```cpp``` 代码块里。\n"
         "3. 避免多余解释，直接实用。"}
    });

    // 添加对话历史
    for (const QJsonValue &msg : conversationHistory)
        messages.append(msg);

    messages.append(QJsonObject{{"role", "user"}, {"content", userText}});
    body["messages"] = messages;

    body["temperature"] = 0.7;
    body["max_tokens"] = 2000;
    body["stream"] = true;

    QNetworkReply* reply = manager->post(request, QJsonDocument(body).toJson());

    // ---------- AI 回复标题 ----------
    cursor = ui->aiChatOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("AI:\n");
    ui->aiChatOutput->setTextCursor(cursor);

    // ---------- 缓冲区（Markdown拼接） ----------
    static QString aiBuffer;

    // ---------- 处理响应 ----------
    connect(reply, &QNetworkReply::readyRead, this, [=]() mutable {
        QByteArray chunk = reply->readAll();
        if (chunk.isEmpty()) return;

        QList<QByteArray> lines = chunk.split('\n');
        for (const QByteArray &line : lines) {
            if (!line.startsWith("data: ")) continue;

            QByteArray jsonPart = line.mid(6).trimmed();
            if (jsonPart == "[DONE]") continue;

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(jsonPart, &parseError);
            if (parseError.error != QJsonParseError::NoError) continue;

            QJsonObject obj = doc.object();
            QJsonArray choices = obj.value("choices").toArray();
            if (choices.isEmpty()) continue;

            QString delta = choices[0].toObject()
                                .value("delta").toObject()
                                .value("content").toString();
            if (delta.isEmpty()) continue;

            // 累积 Markdown 输出
            aiBuffer += delta;

            // 渲染 Markdown
            ui->aiChatOutput->setMarkdown("AI:\n" + aiBuffer);

            // 自动滚动到底部
            QScrollBar *bar = ui->aiChatOutput->verticalScrollBar();
            if (bar) {
                bar->setValue(bar->maximum());
            }

            // 更新对话历史
            if (!conversationHistory.isEmpty() &&
                conversationHistory.last().toObject().value("role").toString() == "assistant") {
                QJsonObject lastObj = conversationHistory.last().toObject();
                lastObj["content"] = lastObj.value("content").toString() + delta;
                conversationHistory[conversationHistory.size() - 1] = lastObj;
            } else {
                conversationHistory.append(QJsonObject{{"role", "assistant"}, {"content", delta}});
            }
        }
    });

    // ---------- 请求完成处理 ----------
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        aiBuffer.clear();
    });
}


void MainWindow::aiImproveCode()
{
    // 检查SSL支持
    qDebug() << "Supports SSL:" << QSslSocket::supportsSsl();
    qDebug() << "Build version:" << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << "Runtime version:" << QSslSocket::sslLibraryVersionString();

    // 获取当前编辑器
    CodeEditor* editor = currentEditor();
    if (!editor) {
        QMessageBox::warning(this, "AI 改代码", "没有打开的文件！");
        return;
    }

    // 获取代码内容
    QString code = editor->toPlainText();

    // 创建网络请求
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("https://api.deepseek.com/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer sk-73a86e4b0df34016b4647887af44ef19");

    QJsonObject body;
    body["model"] = "deepseek-chat";
    body["stream"] = true;

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", "你是一个资深的C/C++开发助手，帮我改进下面的代码，并保持可编译，同时提供文字解释。"}
    });
    messages.append(QJsonObject{ {"role", "user"}, {"content", code} });
    body["messages"] = messages;

    QNetworkReply* reply = manager->post(request, QJsonDocument(body).toJson());

    // 用堆对象保存buffer，确保lambda生命周期正确
    auto buffer = new QString();

    // 处理响应
    connect(reply, &QNetworkReply::readyRead, this, [=]() {
        QByteArray chunk = reply->readAll();
        QList<QByteArray> lines = chunk.split('\n');

        for (auto& l : lines) {
            if (!l.startsWith("data: ")) continue;
            QByteArray jsonData = l.mid(6).trimmed();
            if (jsonData == "[DONE]") {
                qDebug() << "流式输出完成";
                return;
            }

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject()) continue;

            QJsonObject delta = doc["choices"].toArray()[0].toObject()["delta"].toObject();
            QString content = delta["content"].toString();
            if (content.isEmpty()) continue;

            // 累积内容
            *buffer += content;

            int pos;
            while ((pos = buffer->indexOf('\n')) != -1) {
                QString line = buffer->left(pos + 1);
                buffer->remove(0, pos + 1);

                QString trimmed = line.trimmed();
                if (!trimmed.isEmpty() &&
                    !trimmed.endsWith(";") &&
                    !trimmed.endsWith("{") &&
                    !trimmed.endsWith("}"))
                {
                    line = "// " + line;
                }

                QTextCursor cursor(editor->textCursor());
                cursor.movePosition(QTextCursor::End);
                cursor.insertText(line);
                editor->setTextCursor(cursor);
            }
        }
    });

    // 请求完成处理
    connect(reply, &QNetworkReply::finished, this, [=]() {
        // 处理残余内容
        if (!buffer->isEmpty()) {
            QString line = *buffer;
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty() &&
                !trimmed.endsWith(";") &&
                !trimmed.endsWith("{") &&
                !trimmed.endsWith("}"))
            {
                line = "// " + line;
            }
            QTextCursor cursor(editor->textCursor());
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(line);
            editor->setTextCursor(cursor);
        }

        // 清理资源
        delete buffer;
        reply->deleteLater();

        // 错误处理
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, "AI 改代码", "请求失败: " + reply->errorString());
        }
    });
}

void MainWindow::checkEnterPressed()
{
    QString text = ui->aiChatInput->toPlainText();

    // 如果最后一行以回车结束，触发发送
    if(text.endsWith('\n')) {
        text = text.trimmed();  // 去掉多余回车
        if(!text.isEmpty()) {
            sendToAI(text);
        }
        ui->aiChatInput->clear();
    }
}

void MainWindow::clearConversationHistory()
{
    conversationHistory = QJsonArray();  // 清空多轮对话
    ui->aiChatOutput->clear();           // 清空显示区域
}

// ==================== 文件操作 ====================
void MainWindow::newFileInProject()
{
    // 移除欢迎页面（如果存在）
    if (welcomeTabPage) {
        int welcomeIndex = ui->tabWidget->indexOf(welcomeTabPage);
        if (welcomeIndex != -1) {
            ui->tabWidget->removeTab(welcomeIndex);
        }
        welcomeTabPage = nullptr;
    }

    if (currentProjectPath.isEmpty()) {
        newFile();
        return;
    }

    // 选择文件类型
    QStringList types = { "C Source File (*.c)", "C++ Source File (*.cpp)", "Header File (*.h)", "Text File (*.txt)" };
    bool ok;
    QString selectedType = QInputDialog::getItem(this, "新建文件", "选择文件类型:", types, 0, false, &ok);
    if (!ok || selectedType.isEmpty()) return;

    // 确定文件扩展名
    QString ext;
    if (selectedType.contains("*.c")) ext = ".c";
    else if (selectedType.contains("*.cpp")) ext = ".cpp";
    else if (selectedType.contains("*.h")) ext = ".h";
    else ext = ".txt";

    // 创建新文件
    bool created = false;
    QString filename;
    int counter = 1;

    while (!created) {
        filename = QString("%1/NewFile%2%3").arg(currentProjectPath).arg(counter).arg(ext);
        if (!QFile::exists(filename)) {
            QFile file(filename);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out.setEncoding(QStringConverter::Utf8); // Qt 6 写法
                out << "";
                file.close();
                created = true;
            }
        }
        counter++;
    }

    // 打开新文件
    openFileRoutine(filename);

    // 更新项目树
    if (projectModel) {
        projectModel->setRootPath(currentProjectPath);
        ui->projectTree->setRootIndex(projectModel->index(currentProjectPath));
    }

    // 更新标签标题状态
    QWidget *tab = ui->tabWidget->currentWidget();
    if (tab) {
        updateTabTitle(tab, true);
    }

    statusBar()->showMessage("新建文件: " + QFileInfo(filename).fileName(), 2000);
}

void MainWindow::newFile()
{
    // 选择文件类型
    QStringList types = { "C Source File (.c)", "C++ Source File (.cpp)", "Header File (.h)", "Text File (.txt)" };
    bool ok;
    QString selectedType = QInputDialog::getItem(this, "新建文件", "选择文件类型:", types, 0, false, &ok);
    if (!ok || selectedType.isEmpty()) return;

    // 确定文件扩展名
    QString ext;
    if (selectedType == "C Source File (.c)") ext = ".c";
    else if (selectedType == "C++ Source File (.cpp)") ext = ".cpp";
    else if (selectedType == "Header File (.h)") ext = ".h";
    else if (selectedType == "Text File (.txt)") ext = ".txt";
    else ext = ".txt"; // 默认值

    // 计算下一个可用的编号
    int maxNumber = 0;
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        QString tabTitle = ui->tabWidget->tabText(i);

        // 移除可能的修改标记[*]
        QString cleanTitle = tabTitle;
        if (cleanTitle.endsWith("[*]")) {
            cleanTitle.chop(3);
        }

        // 只检查与当前文件类型相同的标签页
        if (cleanTitle.endsWith(ext)) {
            QRegularExpression re("^Untitled\\((\\d+)\\)" + QRegularExpression::escape(ext) + "$");
            QRegularExpressionMatch match = re.match(cleanTitle);
            if (match.hasMatch()) {
                int number = match.captured(1).toInt();
                if (number > maxNumber) {
                    maxNumber = number;
                }
            }
        }
    }

    // 生成带编号的文件标题
    QString title = "Untitled(" + QString::number(maxNumber + 1) + ")" + ext;

    // 创建新标签页
    QWidget *tabContainer = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(tabContainer);
    layout->setSpacing(6);
    layout->setContentsMargins(13, 13, 13, 13);

    CodeEditor *editor = createEditor(tabContainer);
    layout->addWidget(editor);

    int tabIndex = ui->tabWidget->addTab(tabContainer, title);
    ui->tabWidget->setCurrentIndex(tabIndex);
    editor->setFocus();

    // 保存文件信息
    tabFilePaths[tabContainer] = "";
    tabSavedContent[tabContainer] = "";

    // 设置编辑器连接
    setupEditor(editor);

    // 初始状态下，新建文件是已修改状态
    updateTabTitle(tabContainer, true);
}

void MainWindow::openFile()
{
    // 移除欢迎页面（如果存在）
    if (welcomeTabPage) {
        int welcomeIndex = ui->tabWidget->indexOf(welcomeTabPage);
        if (welcomeIndex != -1) {
            ui->tabWidget->removeTab(welcomeIndex);
        }
        welcomeTabPage = nullptr;
    }

    // 选择文件
    QString filename = QFileDialog::getOpenFileName(this, "Open File", "", "C/C++/Text Files (*.c *.cpp *.h *.txt)");
    if (filename.isEmpty()) return;
    filename = QDir::toNativeSeparators(filename);

    // 检查文件是否已打开
    for (auto it = tabFilePaths.constBegin(); it != tabFilePaths.constEnd(); ++it) {
        if (QDir::toNativeSeparators(it.value()) == filename) {
            int index = ui->tabWidget->indexOf(it.key());
            if (index != -1) ui->tabWidget->setCurrentIndex(index);
            QMessageBox::information(this, "提示", "该文件已打开！");
            return;
        }
    }

    // 打开文件
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->outputWindow->appendPlainText("无法打开文件: " + filename);
        return;
    }

    // 读取文件内容
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();

    // 创建新标签页
    QWidget *tabContainer = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(tabContainer);
    layout->setSpacing(6);
    layout->setContentsMargins(13, 13, 13, 13);

    CodeEditor *editor = createEditor(tabContainer);
    editor->setPlainText(content);
    layout->addWidget(editor);
    setupEditor(editor);

    int tabIndex = ui->tabWidget->addTab(tabContainer, QFileInfo(filename).fileName());
    ui->tabWidget->setCurrentIndex(tabIndex);

    // 保存文件信息
    tabFilePaths[tabContainer] = filename;
    tabSavedContent[tabContainer] = content;

    statusBar()->showMessage("Opened: " + filename, 2000);
}

void MainWindow::openFileRoutine(const QString &filePath)
{
     // ✅ 打开前先检查是否已打开过
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        QWidget *tab = ui->tabWidget->widget(i);
        if (tabFilePaths.contains(tab) && tabFilePaths[tab] == filePath) {
            ui->tabWidget->setCurrentIndex(i);  // 切换到已打开的 tab
            QMessageBox::information(this, "提示", "该文件已打开");
            return;  // 不再重复打开
        }
    }

    // 打开文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Open File", "Cannot open file: " + file.errorString());
        return;
    }

    // 读取文件内容
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();

    // 创建新标签页
    QWidget *tabContainer = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(tabContainer);
    layout->setSpacing(6);
    layout->setContentsMargins(13, 13, 13, 13);

    CodeEditor *editor = createEditor(tabContainer);
    editor->setPlainText(content);
    layout->addWidget(editor);
    setupEditor(editor);

    int tabIndex = ui->tabWidget->addTab(tabContainer, QFileInfo(filePath).fileName());
    ui->tabWidget->setCurrentIndex(tabIndex);

    // ✅ 设置 tooltip，显示完整路径
    ui->tabWidget->setTabToolTip(tabIndex, filePath);

    // 保存文件信息
    tabFilePaths[tabContainer] = filePath;
    tabSavedContent[tabContainer] = content;
}


void MainWindow::saveFile()
{
    QWidget *tab = ui->tabWidget->currentWidget();
    if (!tab) return;

    // 获取文件路径
    QString filePath;
    if (tabFilePaths.contains(tab)) {
        filePath = tabFilePaths.value(tab);
    } else if (CodeEditor *editorTab = qobject_cast<CodeEditor*>(tab)) {
        if (tabFilePaths.contains(editorTab))
            filePath = tabFilePaths.value(editorTab);
        else {
            saveFileAs();
            return;
        }
    } else {
        saveFileAs();
        return;
    }

    // 获取编辑器
    CodeEditor *editor = tab->findChild<CodeEditor*>();
    if (!editor) {
        editor = qobject_cast<CodeEditor*>(tab);
    }
    if (!editor) return;

    // 获取内容
    QString content = editor->toPlainText();
    if (filePath.isEmpty()) {
        saveFileAs();
        return;
    }

    // 保存文件
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "保存失败", "无法保存文件：" + filePath);
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    file.close();

    // 更新保存的内容
    tabSavedContent[tab] = content;

    // 更新标签页标题，移除[*]标记
    updateTabTitle(tab, false);

    editor->document()->setModified(false);
    statusBar()->showMessage("已保存: " + QFileInfo(filePath).fileName(), 2000);
}

void MainWindow::saveFileAs()
{
    QWidget *tab = ui->tabWidget->currentWidget();
    if (!tab) return;

    // 获取编辑器
    CodeEditor *editor = tab->findChild<CodeEditor*>();
    if (!editor) {
        editor = qobject_cast<CodeEditor*>(tab);
    }
    if (!editor) return;

    // 获取当前标签页标题，并移除[*]标记
    QString currentTitle = ui->tabWidget->tabText(ui->tabWidget->indexOf(tab));
    if (currentTitle.endsWith("[*]")) {
        currentTitle.chop(3);
    }

    // 从标题中提取文件名（不含扩展名）作为默认名称
    QString baseName = currentTitle;
    if (baseName.contains(".")) {
        baseName = baseName.left(baseName.lastIndexOf("."));
    }

    // 确定默认扩展名
    QString ext = ".txt"; // 默认扩展名
    if (currentTitle.endsWith(".c")) ext = ".c";
    else if (currentTitle.endsWith(".cpp")) ext = ".cpp";
    else if (currentTitle.endsWith(".h")) ext = ".h";

    // 构建默认路径和文件名
    QString defaultPath;
    if (tabFilePaths.contains(tab) && !tabFilePaths.value(tab).isEmpty()) {
        defaultPath = tabFilePaths.value(tab);
    } else {
        defaultPath = currentProjectPath.isEmpty() ? "" : currentProjectPath + "/";
        defaultPath += baseName + ext;
    }

    // 弹出保存对话框
    QString filename = QFileDialog::getSaveFileName(
        this,
        "另存为",
        defaultPath,
        "C/C++/Text Files (*.c *.cpp *.h *.txt);;C Files (*.c);;C++ Files (*.cpp);;Header Files (*.h);;Text Files (*.txt);;All Files (*)"
        );

    if (filename.isEmpty()) return;

    // 转换为本地分隔符格式
    filename = QDir::toNativeSeparators(filename);

    // 确保文件有正确的扩展名
    QFileInfo fileInfo(filename);
    if (fileInfo.suffix().isEmpty()) {
        filename += ext;
    }

    // 保存文件
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法保存文件: " + filename);
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << editor->toPlainText();
    file.close();

    // 更新文件路径和保存内容
    tabFilePaths[tab] = filename;
    tabSavedContent[tab] = editor->toPlainText();

    // 更新标签页标题，移除[*]标记
    updateTabTitle(tab, false);

    // 刷新项目树（如果有项目打开）
    if (projectModel && !currentProjectPath.isEmpty()) {
        projectModel->setRootPath(currentProjectPath);
        ui->projectTree->setRootIndex(projectModel->index(currentProjectPath));
    }

    editor->document()->setModified(false);
    statusBar()->showMessage("另存为成功: " + filename, 2000);
}

// ==================== 编辑器管理 ====================
void MainWindow::onEditorTextChanged()
{
    // 获取发送信号的编辑器
    CodeEditor *editor = qobject_cast<CodeEditor*>(sender());
    if (!editor) return;

    // 找到包含此编辑器的标签页
    QWidget *tab = nullptr;
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        QWidget *currentTab = ui->tabWidget->widget(i);
        CodeEditor *currentEditor = currentTab->findChild<CodeEditor*>();
        if (!currentEditor) {
            currentEditor = qobject_cast<CodeEditor*>(currentTab);
        }
        if (currentEditor == editor) {
            tab = currentTab;
            break;
        }
    }

    if (!tab) return;

    // 获取当前内容
    QString currentContent = editor->toPlainText();

    // 检查内容是否已修改
    bool modified = currentContent != tabSavedContent.value(tab);

    // 更新标签页标题
    updateTabTitle(tab, modified);
}

void MainWindow::setupEditor(CodeEditor *editor)
{
    connect(editor, &CodeEditor::textChanged, this, &MainWindow::onEditorTextChanged);
}

void MainWindow::updateTabTitle(QWidget *tab, bool modified)
{
    if (!tab) return;

    int tabIndex = ui->tabWidget->indexOf(tab);
    if (tabIndex == -1) return;

    QString title;
    QString filePath = tabFilePaths.value(tab);

    if (filePath.isEmpty()) {
        // 对于未保存的文件，使用当前标签页标题（移除可能的[*]标记）
        title = ui->tabWidget->tabText(tabIndex);
        if (title.endsWith("[*]")) {
            title.chop(3);
        }
    } else {
        // 对于已保存的文件，使用文件名
        title = QFileInfo(filePath).fileName();
    }

    // 添加修改标记
    if (modified) {
        title += "[*]";
    }

    ui->tabWidget->setTabText(tabIndex, title);
}

// ==================== 项目操作 ====================
void MainWindow::chooseProjectDirectory(const QString &defaultPath)
{
    // 选择项目目录
    QString dir = QFileDialog::getExistingDirectory(this, "选择工程目录", defaultPath);
    if (dir.isEmpty()) return;

    // 检查未保存文件
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        QWidget* tab = ui->tabWidget->widget(i);
        CodeEditor* editor = qobject_cast<CodeEditor*>(tab);
        if (!editor) editor = tab->findChild<CodeEditor*>();
        if (!editor) continue;

        QString savedContent = tabSavedContent.value(tab, "");
        if (editor->toPlainText() != savedContent) {
            ui->tabWidget->setCurrentWidget(tab);
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "未保存的更改",
                QString("文件 %1 有未保存的更改，是否保存？").arg(ui->tabWidget->tabText(i)),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                );

            if (reply == QMessageBox::Yes) {
                saveFile();
            } else if (reply == QMessageBox::Cancel) {
                return;
            }
        }
    }

    // 清理旧项目
    while (ui->tabWidget->count() > 0) {
        closeTab(0);
    }
    tabFilePaths.clear();
    tabSavedContent.clear();

    if (projectModel) {
        projectModel->deleteLater();
        projectModel = nullptr;
    }

    currentProjectPath = dir;

    // 加载新项目
    projectModel = new QFileSystemModel(this);
    projectModel->setRootPath(dir);
    projectModel->setNameFilters(QStringList() << "*.cpp" << "*.c" << "*.h");
    projectModel->setNameFilterDisables(false);

    ui->projectTree->setModel(projectModel);
    ui->projectTree->setRootIndex(projectModel->index(dir));

    ui->projectTree->disconnect();

    connect(ui->projectTree, &QTreeView::doubleClicked, this, [=](const QModelIndex &index) {
        QString path = projectModel->filePath(index);
        QFileInfo info(path);
        if (info.isFile()) openFileRoutine(path);
    });

    // 显示项目名称和路径
    QString projectName = QFileInfo(currentProjectPath).fileName();
    setWindowTitle(QString("CIDE - %1 [%2]").arg(projectName).arg(currentProjectPath));

    statusBar()->showMessage("已切换到项目: " + currentProjectPath, 2000);
}

void MainWindow::createProject()
{
    // 选择项目保存目录
    QString dir = QFileDialog::getExistingDirectory(this, "选择新项目保存目录");
    if (dir.isEmpty()) return;

    // 输入项目名称
    bool ok;
    QString projectName = QInputDialog::getText(this, "新建项目", "项目名称:", QLineEdit::Normal, "", &ok);
    if (!ok || projectName.isEmpty()) return;

    QDir projectDir(dir);
    QString projectPath = projectDir.filePath(projectName);

    // 检查项目是否已存在
    if (QDir(projectPath).exists()) {
        QMessageBox::warning(this, "错误", "项目已存在！");
        return;
    }

    // 创建项目目录
    if (!projectDir.mkdir(projectName)) {
        QMessageBox::warning(this, "错误", "创建项目失败");
        return;
    }

    // 设置当前项目路径
    currentProjectPath = projectPath;

    // 创建main.cpp文件
    QString mainFilePath = currentProjectPath + "/main.cpp";
    QFile file(mainFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << "#include <iostream>\n\nint main() {\n    std::cout << \"Hello World!\" << std::endl;\n    return 0;\n}\n";
        file.close();
    }

    // 手动加载新项目
    QString dirToLoad = currentProjectPath;

    // 清理旧项目
    while (ui->tabWidget->count() > 0) closeTab(0);
    tabFilePaths.clear();
    tabSavedContent.clear();

    if (projectModel) {
        projectModel->deleteLater();
        projectModel = nullptr;
    }

    // 加载新项目
    projectModel = new QFileSystemModel(this);
    projectModel->setRootPath(dirToLoad);
    projectModel->setNameFilters(QStringList() << "*.cpp" << "*.c" << "*.h");
    projectModel->setNameFilterDisables(false);

    ui->projectTree->setModel(projectModel);
    ui->projectTree->setRootIndex(projectModel->index(dirToLoad));

    ui->projectTree->disconnect();
    connect(ui->projectTree, &QTreeView::doubleClicked, this, [=](const QModelIndex &index) {
        QString path = projectModel->filePath(index);
        QFileInfo info(path);
        if (info.isFile()) openFileRoutine(path);
    });

    // 显示项目名称和路径
    QString displayName = QFileInfo(currentProjectPath).fileName();
    setWindowTitle(QString("CIDE - %1 [%2]").arg(displayName).arg(currentProjectPath));
    statusBar()->showMessage("已切换到项目: " + currentProjectPath, 2000);

    // 打开main.cpp
    openFileRoutine(mainFilePath);
}

// ==================== 编辑器获取和工具函数 ====================
CodeEditor* MainWindow::currentEditor()
{
    QWidget *tab = ui->tabWidget->currentWidget();
    if (!tab) return nullptr;

    CodeEditor *editor = qobject_cast<CodeEditor*>(tab);
    if (editor) return editor;

    return tab->findChild<CodeEditor*>();
}

QStringList MainWindow::collectSourceFiles(const QString &dirPath)
{
    QStringList files;
    QDir dir(dirPath);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : entries) {
        if (info.isDir()) {
            files.append(collectSourceFiles(info.absoluteFilePath()));
        } else if (info.suffix() == "cpp" || info.suffix() == "c") {
            files.append(info.absoluteFilePath());
        }
    }
    return files;
}

CodeEditor* MainWindow::createEditor(QWidget *parent)
{
    CodeEditor *editor = new CodeEditor(parent);
    return editor;
}

// ==================== 编辑功能 ====================
void MainWindow::setFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, this);
    if (!ok) return;

    CodeEditor *editor = currentEditor();
    if (!editor) return;

    editor->setFont(font);
    editor->update();
}

void MainWindow::setColor()
{
    QColor color = QColorDialog::getColor(Qt::black, this);
    if (!color.isValid()) return;

    CodeEditor *editor = currentEditor();
    if (!editor) return;

    QPalette pal = editor->palette();
    pal.setColor(QPalette::Text, color);
    editor->setPalette(pal);
}

void MainWindow::exitApp()
{
    QApplication::quit();
}

// ==================== 查找功能 ====================
void MainWindow::findText()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return;

    bool ok;
    QString search = QInputDialog::getText(this, "Find", "Enter text to find:", QLineEdit::Normal, lastSearchText, &ok);
    if (!ok || search.isEmpty()) return;

    lastSearchText = search;
    QString content = editor->toPlainText();
    QTextCursor cursor(editor->document());

    // 清除之前的高亮
    QTextCharFormat clearFormat;
    clearFormat.setBackground(Qt::transparent);
    cursor.select(QTextCursor::Document);
    cursor.setCharFormat(clearFormat);

    // 设置高亮格式
    QTextCharFormat format;
    format.setBackground(Qt::yellow);

    // 查找所有匹配项
    searchResults.clear();
    int pos = 0;
    while ((pos = content.indexOf(search, pos, Qt::CaseSensitive)) != -1) {
        QTextCursor highlightCursor(editor->document());
        highlightCursor.setPosition(pos);
        highlightCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, search.length());
        highlightCursor.setCharFormat(format);
        searchResults.append(highlightCursor);
        pos += search.length();
    }

    if (searchResults.isEmpty()) {
        QMessageBox::information(this, "Find", "Text not found.");
        currentResultIndex = -1;
    } else {
        currentResultIndex = 0;
        editor->setTextCursor(searchResults[currentResultIndex]);
        editor->setFocus();
    }
}

void MainWindow::findNext()
{
    CodeEditor *editor = currentEditor();
    if (!editor || searchResults.isEmpty()) return;

    currentResultIndex = (currentResultIndex + 1) % searchResults.size();
    editor->setTextCursor(searchResults[currentResultIndex]);
    editor->setFocus();
}

void MainWindow::findPrevious()
{
    CodeEditor *editor = currentEditor();
    if (!editor || searchResults.isEmpty()) return;

    currentResultIndex = (currentResultIndex - 1 + searchResults.size()) % searchResults.size();
    editor->setTextCursor(searchResults[currentResultIndex]);
    editor->setFocus();
}

// ==================== 标签页管理 ====================
void MainWindow::closeTab(int index)
{
    QWidget *tab = ui->tabWidget->widget(index);
    if (!tab) return;

    CodeEditor *editor = qobject_cast<CodeEditor*>(tab);
    if (!editor) editor = tab->findChild<CodeEditor*>();
    if (!editor) {
        ui->tabWidget->removeTab(index);
        tabFilePaths.remove(tab);
        tabSavedContent.remove(tab);
        tab->deleteLater();
        return;
    }

    // 检查是否有未保存的更改
    if (!editor->document()->isModified() ||
        tabSavedContent.value(tab, QString()) == editor->toPlainText()) {
        ui->tabWidget->removeTab(index);
        tabFilePaths.remove(tab);
        tabSavedContent.remove(tab);
        tab->deleteLater();
        return;
    }

    // 询问是否保存
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "未保存的更改",
                                  "此文档有未保存的更改，是否保存？",
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (reply == QMessageBox::Yes) {
        ui->tabWidget->setCurrentWidget(tab);
        saveFile();
        if (!editor->document()->isModified()) {
            ui->tabWidget->removeTab(index);
            tabFilePaths.remove(tab);
            tabSavedContent.remove(tab);
            tab->deleteLater();
        }
    } else if (reply == QMessageBox::No) {
        ui->tabWidget->removeTab(index);
        tabFilePaths.remove(tab);
        tabSavedContent.remove(tab);
        tab->deleteLater();
    }
}

// ==================== 编译和运行 ====================
void MainWindow::compileCurrentFile()
{
    saveFile();

    QStringList filesToCompile;

    // 收集要编译的文件
    if (!currentProjectPath.isEmpty()) {
        filesToCompile = collectSourceFiles(currentProjectPath);
        if (filesToCompile.isEmpty()) {
            QMessageBox::warning(this, "提示", "项目中没有源文件！");
            return;
        }
    } else {
        QWidget* tab = ui->tabWidget->currentWidget();
        if (!tab) {
            QMessageBox::warning(this, "提示", "没有可编译的文件！");
            return;
        }

        QString filePath = tabFilePaths.value(tab);
        if (filePath.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先保存文件后再编译！");
            return;
        }

        filesToCompile << filePath;
    }

    // 设置编译路径
    QString appDir = QCoreApplication::applicationDirPath();
    QString exePath = QDir(appDir).filePath("temp.exe");
    QString gppPath = QDir(appDir).filePath("mingw/bin/g++.exe");

    // 清空输出窗口并显示编译信息
    ui->outputWindow->clear();
    ui->outputWindow->appendPlainText("🔨 正在编译...");

    // 删除旧的执行文件
    if (QFile::exists(exePath)) QFile::remove(exePath);

    // 设置编译环境
    QProcess compileProcess;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", env.value("PATH") + ";" + QDir(appDir).filePath("mingw/bin"));
    compileProcess.setProcessEnvironment(env);

    // 构建编译参数
    QStringList args;
    for (const QString &f : filesToCompile)
        args << QDir::toNativeSeparators(f);

    args << "-o" << QDir::toNativeSeparators(exePath);

    // 执行编译
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    compileProcess.start(gppPath, args);
    compileProcess.waitForFinished();
    qint64 endTime = QDateTime::currentMSecsSinceEpoch();

    // 获取编译输出
    QString output = compileProcess.readAllStandardOutput();
    QString errors = compileProcess.readAllStandardError();

    if (!output.isEmpty()) ui->outputWindow->appendPlainText(output.trimmed());
    if (!errors.isEmpty()) ui->outputWindow->appendPlainText(errors.trimmed());

    // 显示编译结果
    double elapsedSec = (endTime - startTime) / 1000.0;

    if (compileProcess.exitCode() != 0)
        ui->outputWindow->appendPlainText("❌ 编译失败！");
    else
        ui->outputWindow->appendPlainText(
            QString("✅ 编译成功，生成：%1 （耗时 %2 秒）")
                .arg(exePath)
                .arg(elapsedSec, 0, 'f', 2)
            );

    ui->outputWindow->appendPlainText("=== Compile Finished ===");
}

void MainWindow::runCurrentFile()
{
    saveFile();

    QString appDir = QCoreApplication::applicationDirPath();
    QString exePath = QDir(appDir).filePath("temp.exe");

    // 如果可执行文件不存在，先编译
    if (!QFile::exists(exePath)) {
        compileCurrentFile();
        if (!QFile::exists(exePath)) return;
    }

    // 运行程序
#ifdef Q_OS_WIN
    QStringList runArgs;
    runArgs << "/C" << "start" << "cmd" << "/K"
            << "chcp 65001 > nul && " + exePath;

    if (!QProcess::startDetached("cmd.exe", runArgs)) {
        ui->outputWindow->appendPlainText("❌ 无法启动程序！");
    }
#else
    if (!QProcess::startDetached(exePath, QStringList(), QFileInfo(exePath).absolutePath())) {
        ui->outputWindow->appendPlainText("❌ 无法启动程序！");
    }
#endif
}

// ==================== 标签页右键菜单 ====================
void MainWindow::showTabContextMenu(const QPoint &pos)
{
    int index = ui->tabWidget->tabBar()->tabAt(pos);
    if (index == -1) return;

    QMenu menu;
    QAction *renameAction = menu.addAction("重命名文件");
    QAction *selectedAction = menu.exec(ui->tabWidget->tabBar()->mapToGlobal(pos));
    if (selectedAction == renameAction) {
        renameTabFile(index);
    }
}

void MainWindow::renameTabFile(int index)
{
    QWidget *tab = ui->tabWidget->widget(index);
    if (!tab) return;

    QString oldPath = tabFilePaths.value(tab, "");
    QString oldName = oldPath.isEmpty() ? ui->tabWidget->tabText(index) : QFileInfo(oldPath).fileName();

    bool ok;
    QString newName = QInputDialog::getText(this, "重命名文件",
                                            "请输入新文件名:",
                                            QLineEdit::Normal,
                                            oldName, &ok);
    if (!ok || newName.isEmpty()) return;

    // 重命名文件
    if (!oldPath.isEmpty()) {
        QFileInfo info(oldPath);
        QString newPath = info.absolutePath() + "/" + newName;

        if (QFile::exists(newPath)) {
            QMessageBox::warning(this, "错误", "文件已存在！");
            return;
        }

        QFile::rename(oldPath, newPath);
        tabFilePaths[tab] = newPath;
    }

    // 更新标签页标题
    ui->tabWidget->setTabText(index, newName);
    statusBar()->showMessage("重命名成功: " + newName, 2000);
}
