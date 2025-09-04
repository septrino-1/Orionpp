#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <qmenu.h>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QPalette pal = a.palette();


    pal.setColor(QPalette::Window, Qt::white);            // 窗口背景
    pal.setColor(QPalette::WindowText, Qt::black);        // 窗口文字
    pal.setColor(QPalette::Base, Qt::white);              // 输入框/文本编辑背景
    pal.setColor(QPalette::Text, Qt::black);              // 输入框文字
    pal.setColor(QPalette::Button, QColor(240, 240, 240));// 按钮背景，浅灰色
    pal.setColor(QPalette::ButtonText, Qt::black);      // 按钮文字
    a.setStyleSheet(R"(
    /* ===== 菜单整体（白底 + 浅蓝边框） ===== */
    QMenu {
        background-color: #F9FBFD;        /* 淡蓝白底 */
        border: 1px solid #87CEFA;        /* 蓝色边框 */
    }

    /* ===== 普通选项 ===== */
    QMenu::item {
        padding: 6px 20px;
        background-color: transparent;
        color: #003366;                   /* 深蓝文字 */
    }

    /* ===== 选中项 ===== */
    QMenu::item:selected {
        background-color: #87CEFA;        /* 蓝色高亮 */
        color: #FFFFFF;                   /* 白色文字 */
    }

    /* ===== 分隔线 ===== */
    QMenu::separator {
        height: 1px;
        background: #87CEFA;
        margin: 4px 8px;
    }

    /* ===== 子菜单小箭头 ===== */
    QMenu::indicator {
        width: 12px;
        height: 12px;
    }
)");


    a.setPalette(pal);



    MainWindow w;
    w.setWindowTitle("CIDE - Orion++");
    w.showMaximized(); // 打开时全屏显示，但保留窗口装饰
    w.show();

    return a.exec();
}
