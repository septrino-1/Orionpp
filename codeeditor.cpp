#include "codeeditor.h"
#include <QPainter>
#include <QTextBlock>
#include "CppHighlighter.h"
#include <QStack>
#include <QPair>
#include <QTimer>

#include <QFontDatabase>

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    // ===== 注册 JetBrains Mono 字体 =====
    int id = QFontDatabase::addApplicationFont(":/new/prefix2/fonts/JetBrainsMonoNL-Bold.ttf");
    if (id != -1) {
        QString family = QFontDatabase::applicationFontFamilies(id).at(0);

        QFont font(family);
        font.setPointSize(14);           // 可根据喜好调整
        font.setStyleHint(QFont::Monospace);
        font.setFixedPitch(true);        // JetBrains Mono 是等宽字体
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        font.setStyleStrategy(QFont::PreferAntialias);
        font.setKerning(true);
#endif
        setFont(font);
        qDebug() << "Loaded font:" << family;

    } else {
        qDebug() << "Failed to load JetBrains Mono font, fallback to system default";
    }


    // ===== 信号槽绑定 =====
    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);


    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    new CppHighlighter(this->document());
}

void CodeEditor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // 获取当前字体
        QFont f = this->font();
        int pointSize = f.pointSize();

        if (event->angleDelta().y() > 0) {
            pointSize++;
        } else {
            pointSize--;
        }

        if (pointSize < 6) pointSize = 6;
        if (pointSize > 40) pointSize = 40;

        f.setPointSize(pointSize);
        this->setFont(f);

        // ===== 关键：更新行号区域宽度 =====
        updateLineNumberAreaWidth(0);

        event->accept();
    } else {
        QPlainTextEdit::wheelEvent(event);
    }
}


int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) { max /= 10; ++digits; }
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (dy == 0)
        lineNumberArea->update();
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

// ---------------- 行高亮 + 括号匹配高亮 ----------------

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    // 当前行高亮
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection lineSel;
        lineSel.format.setBackground(QColor(Qt::yellow).lighter(160));
        lineSel.format.setProperty(QTextFormat::FullWidthSelection, true);
        lineSel.cursor = textCursor();
        lineSel.cursor.clearSelection();
        extraSelections.append(lineSel);
    }

    QString text = document()->toPlainText();
    int pos = textCursor().position();
    if (text.isEmpty() || pos < 0 || pos >= text.size()) return;

    // 只检查光标直接所在的字符是否是括号
    QChar charAtPos = text.at(pos);
    bool isBracket = (charAtPos == '(' || charAtPos == ')' ||
                      charAtPos == '{' || charAtPos == '}' ||
                      charAtPos == '[' || charAtPos == ']');

    // 如果不是括号，直接返回
    if (!isBracket) {
        setExtraSelections(extraSelections);
        return;
    }

    // 查找匹配的括号
    int matchPos = findMatchingBracket(text, pos);
    if (matchPos == -1) {
        setExtraSelections(extraSelections);
        return;
    }

    // 高亮匹配的括号
    auto makeSelection = [&](int p) -> QTextEdit::ExtraSelection {
        QTextEdit::ExtraSelection sel;
        QTextCursor cursor(document());
        cursor.setPosition(p);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        sel.cursor = cursor;
        sel.format.setBackground(QColor(Qt::green).lighter(160));
        sel.format.setFontWeight(QFont::Bold);
        return sel;
    };

    extraSelections.append(makeSelection(pos));
    extraSelections.append(makeSelection(matchPos));

    setExtraSelections(extraSelections);
}

int CodeEditor::findMatchingBracket(const QString& text, int pos)
{
    if (pos < 0 || pos >= text.length()) return -1;

    QChar bracket = text.at(pos);

    // 确定括号类型和搜索方向
    QChar targetBracket;
    int direction;

    if (bracket == '(') {
        targetBracket = ')';
        direction = 1;
    } else if (bracket == ')') {
        targetBracket = '(';
        direction = -1;
    } else if (bracket == '{') {
        targetBracket = '}';
        direction = 1;
    } else if (bracket == '}') {
        targetBracket = '{';
        direction = -1;
    } else if (bracket == '[') {
        targetBracket = ']';
        direction = 1;
    } else if (bracket == ']') {
        targetBracket = '[';
        direction = -1;
    } else {
        return -1; // 不是括号
    }

    int depth = 0;
    int i = pos + direction;

    enum State { Normal, InString, InChar, InLineComment, InBlockComment };
    State state = Normal;
    QChar stringQuote;

    while (i >= 0 && i < text.length()) {
        QChar c = text.at(i);

        // 处理状态转换
        switch (state) {
        case Normal:
            if (c == '"' || c == '\'') {
                state = (c == '"') ? InString : InChar;
                stringQuote = c;
            } else if (c == '/' && i+1 < text.length()) {
                if (text.at(i+1) == '/') {
                    state = InLineComment;
                    i++; // 跳过下一个字符
                } else if (text.at(i+1) == '*') {
                    state = InBlockComment;
                    i++; // 跳过下一个字符
                }
            } else if (c == bracket) {
                // 遇到同类型的括号，增加深度
                depth++;
            } else if (c == targetBracket) {
                if (depth == 0) {
                    // 找到匹配的括号
                    return i;
                }
                depth--; // 减少深度
            }
            break;

        case InString:
        case InChar:
            if (c == stringQuote && (i == 0 || text.at(i-1) != '\\')) {
                state = Normal;
            }
            break;

        case InLineComment:
            if (c == '\n') {
                state = Normal;
            }
            break;

        case InBlockComment:
            if (c == '*' && i+1 < text.length() && text.at(i+1) == '/') {
                state = Normal;
                i++; // 跳过下一个字符
            }
            break;
        }

        i += direction;
    }

    return -1; // 没有找到匹配的括号
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int topMargin = contentOffset().y();
    int top = (int)blockBoundingGeometry(block).translated(0, topMargin).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width() - 2, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        ++blockNumber;
    }
}

// ---------------- 自动补全括号 ----------------
void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    QTextCursor cursor = textCursor();
    QChar ch = event->text().isEmpty() ? QChar() : event->text().at(0);

    // 缩进单位（你可以改成 "\t" 或更短/更长的空格）
    const QString indentUnit = "\t";  // 一个制表符


    // ---------- Enter: 处理光标在配对括号中间的智能换行 ----------
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        int pos = cursor.position();
        QChar leftChar = (pos > 0) ? document()->characterAt(pos - 1) : QChar();
        QChar rightChar = document()->characterAt(pos);

        QMap<QChar, QChar> pairs = {
            {'(', ')'},
            {'{', '}'},
            {'[', ']'}
        };

        // 情况 A: 光标位于一对已存在的配对括号之间 -> 插入三行结构并把光标放在中间行
        if (pairs.contains(leftChar) && pairs[leftChar] == rightChar) {
            // 当前行前导空白
            QString currentLine = cursor.block().text();
            QString baseIndent;
            for (QChar c : currentLine) {
                if (c == ' ' || c == '\t') baseIndent.append(c);
                else break;
            }

            // 插入文本：\n + baseIndent + indentUnit  （用户编写代码的行）
            //             \n + baseIndent          （右括号所在行）
            int insertPos = cursor.position();
            QString toInsert = "\n" + baseIndent + indentUnit + "\n" + baseIndent;

            cursor.beginEditBlock();
            cursor.insertText(toInsert);
            cursor.endEditBlock();

            // 将光标放在中间那行（也就是插入点后的：1 + baseIndent + indentUnit）
            int desiredPos = insertPos + 1 + baseIndent.length() + indentUnit.length();
            QTextCursor newCursor(document());
            newCursor.setPosition(desiredPos);
            setTextCursor(newCursor);
            return;
        }

        // 情况 B: 普通换行时继承缩进，如果上一行以 '{' 结尾则额外缩进
        QString currentLine = cursor.block().text();
        QString baseIndent;
        for (QChar c : currentLine) {
            if (c == ' ' || c == '\t') baseIndent.append(c);
            else break;
        }

        // 先执行默认换行
        QPlainTextEdit::keyPressEvent(event);

        // 插入继承的缩进
        cursor = textCursor();
        cursor.insertText(baseIndent);

        // 如果上一行以 { 结尾（忽略尾部空白），再额外插入一个缩进单位
        if (currentLine.trimmed().endsWith("{")) {
            cursor.insertText(indentUnit);
        }

        setTextCursor(cursor);
        return;
    }

    // ---------- Tab: 缩进多行或插入缩进单位 ----------
    if (event->key() == Qt::Key_Tab && !(event->modifiers() & Qt::ShiftModifier)) {
        cursor = textCursor();
        if (cursor.hasSelection()) {
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();

            cursor.beginEditBlock();

            QTextCursor tmp(document());
            tmp.setPosition(start);
            int firstBlock = tmp.blockNumber();
            tmp.setPosition(end);
            int lastBlock = tmp.blockNumber();

            for (int i = firstBlock; i <= lastBlock; ++i) {
                QTextBlock block = document()->findBlockByNumber(i);
                QTextCursor c(block);
                c.movePosition(QTextCursor::StartOfBlock);
                c.insertText(indentUnit);
            }

            cursor.endEditBlock();
        } else {
            // 没有选中则直接插入缩进单位
            QPlainTextEdit::keyPressEvent(event); // 插入由系统的 text() 提供的字符（若为空则我们手动）
            if (event->text().isEmpty()) {
                cursor.insertText(indentUnit);
                setTextCursor(cursor);
            }
        }
        return;
    }

    // ---------- Shift+Tab 或 Backtab: 反缩进 ----------
    if (event->key() == Qt::Key_Backtab || (event->key() == Qt::Key_Tab && (event->modifiers() & Qt::ShiftModifier))) {
        cursor = textCursor();
        if (cursor.hasSelection()) {
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();

            cursor.beginEditBlock();

            QTextCursor tmp(document());
            tmp.setPosition(start);
            int firstBlock = tmp.blockNumber();
            tmp.setPosition(end);
            int lastBlock = tmp.blockNumber();

            for (int i = firstBlock; i <= lastBlock; ++i) {
                QTextBlock block = document()->findBlockByNumber(i);
                QString line = block.text();
                QTextCursor c(block);
                c.movePosition(QTextCursor::StartOfBlock);

                if (line.startsWith(indentUnit)) {
                    c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, indentUnit.length());
                    c.removeSelectedText();
                } else if (line.startsWith("\t")) {
                    c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 1);
                    c.removeSelectedText();
                } else {
                    // 若既不是完整的 indentUnit，也不是 tab，则尽量删除前导空格（最多 indentUnit.length() 个）
                    int removeCount = 0;
                    for (int k = 0; k < line.length() && k < indentUnit.length(); ++k) {
                        if (line[k] == ' ') ++removeCount;
                        else break;
                    }
                    if (removeCount > 0) {
                        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, removeCount);
                        c.removeSelectedText();
                    }
                }
            }

            cursor.endEditBlock();
        }
        return;
    }

    // ---------- 自动补全括号（保留你原有逻辑） ----------
    QMap<QChar, QChar> brackets = {
        {'(', ')'},
        {'[', ']'},
        {'{', '}'},
        {'"', '"'},
        {'\'', '\''},
        {'<', '>'}
    };

    if (brackets.contains(ch)) {
        // 插入左括号（或引号）
        QPlainTextEdit::keyPressEvent(event);
        // 插入匹配的右括号
        QChar right = brackets[ch];
        cursor = textCursor();
        cursor.insertText(right);
        // 将光标移回到两者中间
        cursor.movePosition(QTextCursor::Left);
        setTextCursor(cursor);
        return;
    }

    // 如果输入的是右括号并且右侧已有相同的右括号，则跳过插入直接移动光标
    if (brackets.values().contains(ch)) {
        cursor = textCursor();
        if (!cursor.atEnd() && cursor.document()->characterAt(cursor.position()) == ch) {
            cursor.movePosition(QTextCursor::Right);
            setTextCursor(cursor);
            return;
        }
    }

    // 其余按键按默认处理
    QPlainTextEdit::keyPressEvent(event);
}

// ---------------- LineNumberArea ----------------
LineNumberArea::LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor)
{
    setMouseTracking(true);
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    codeEditor->lineNumberAreaPaintEvent(event);
}

bool LineNumberArea::event(QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress ||
        e->type() == QEvent::MouseButtonDblClick ||
        e->type() == QEvent::MouseButtonRelease) {
        return true;
    }
    return QWidget::event(e);
}
