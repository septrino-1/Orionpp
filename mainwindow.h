#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QMap>
#include <QWidget>
#include <QProcess>
#include "codeeditor.h"
#include <QFileSystemModel>
#include <QNetworkAccessManager>
#include <QJsonArray>
#include <QAction>
#include <QSettings>

QT_BEGIN_NAMESPACE
namespace Ui { class mainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // ==================== 初始化函数 ====================
    void setupConnections();
    void setupStatusBar();
    void setupOutputWindow();
    void setupProjectTree();
    void setupUI();
    void setupEditor(CodeEditor *editor);
    void setupWelcomeTab();

    // ==================== 文件操作 ====================
    void newFile();
    void newFileInProject();
    void openFileRoutine(const QString &filePath);
    void openFile();
    void saveFile();
    void saveFileAs();
    void exitApp();
    void chooseProjectDirectory(const QString &defaultPath = "");
    void createProject();
    void onEditorTextChanged();
    void updateTabTitle(QWidget *tab, bool modified);

    // ==================== 编辑操作 ====================
    void setFont();
    void setColor();
    void closeTab(int index);
    void findText();
    void findNext();
    void findPrevious();

    // ==================== 编译运行 ====================
    void compileCurrentFile();
    void runCurrentFile();
    QStringList collectSourceFiles(const QString &dirPath);

    // ==================== 标签页管理 ====================
    void showTabContextMenu(const QPoint &pos);
    void renameTabFile(int index);

    // ==================== AI功能 ====================
    void aiImproveCode();
    void checkEnterPressed();
    void sendToAI(const QString &userText);
    void clearConversationHistory();
    //用户手册
    void showHelp();

private:
    Ui::mainWindow *ui;

    QWidget *welcomeTabPage = nullptr;
    // ==================== 编辑器管理 ====================
    CodeEditor* createEditor(QWidget *parent);
    CodeEditor* currentEditor();
    QMap<QWidget*, QString> tabFilePaths;    // 存储每个 tab 对应的文件路径
    QMap<QWidget*, QString> tabSavedContent; // tab -> 上次保存的文本

    // ==================== 进程和路径管理 ====================
    QProcess *process = nullptr;         // 用于编译和运行
    QString inputLine;
    QString currentFilePath;
    QString currentProjectPath;   // 当前项目根目录

    // ==================== 查找功能 ====================
    QString lastSearchText;
    QList<QTextCursor> searchResults;
    int currentResultIndex = -1;

    // ==================== 项目模型和网络 ====================
    QFileSystemModel* projectModel = nullptr;
    QNetworkAccessManager *manager;
    QJsonArray conversationHistory; // 保存多轮对话历史
};

#endif // MAINWINDOW_H
