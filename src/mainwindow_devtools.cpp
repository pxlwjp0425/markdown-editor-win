/*
 * MainWindow —— 开发者工具与对话框。
 *
 * 偏好设置、快捷键速查、关于、打印、开发者视图、性能打点。
 * 这些都是低频使用的界面功能，混在主窗口里会淹没主线逻辑。
 */

#include "mainwindow.h"
#include <QMessageBox>
#include <QFontDialog>
#include <QColorDialog>
#include <QPrintDialog>
#include <QPrinter>
#include <QFormLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QUrl>

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("关于 Markdown Editor"),
        tr("<h3>Markdown Editor v1.0</h3>"
           "<p>一个功能强大的 Markdown 编辑器</p>"
           "<p>特性：</p>"
           "<ul>"
           "<li>实时预览</li>"
           "<li>语法高亮</li>"
           "<li>快捷键支持</li>"
           "<li>文件管理</li>"
           "<li>深色模式</li>"
           "<li>导出 HTML/PDF</li>"
           "</ul>"));
}

void MainWindow::showShortcuts()
{
    QString shortcuts = tr(
        "<h3>快捷键列表</h3>"
        "<table>"
        "<tr><td><b>Ctrl+N</b></td><td>新建文件</td></tr>"
        "<tr><td><b>Ctrl+O</b></td><td>打开文件</td></tr>"
        "<tr><td><b>Ctrl+S</b></td><td>保存文件</td></tr>"
        "<tr><td><b>Ctrl+Shift+S</b></td><td>另存为</td></tr>"
        "<tr><td><b>Ctrl+B</b></td><td>粗体</td></tr>"
        "<tr><td><b>Ctrl+I</b></td><td>斜体</td></tr>"
        "<tr><td><b>Ctrl+K</b></td><td>插入链接</td></tr>"
        "<tr><td><b>Ctrl+E</b></td><td>行内代码</td></tr>"
        "<tr><td><b>Ctrl+F</b></td><td>查找</td></tr>"
        "<tr><td><b>Ctrl+H</b></td><td>替换</td></tr>"
        "<tr><td><b>F11</b></td><td>全屏</td></tr>"
        "<tr><td><b>Ctrl++</b></td><td>放大</td></tr>"
        "<tr><td><b>Ctrl+-</b></td><td>缩小</td></tr>"
        "</table>"
    );
    
    QMessageBox::information(this, tr("快捷键"), shortcuts);
}

void MainWindow::showPreferences()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("偏好设置"));
    dlg.setMinimumWidth(420);

    QFormLayout *form = new QFormLayout();
    auto *fontSpin = new QSpinBox(&dlg);
    fontSpin->setRange(8, 48);
    fontSpin->setValue(m_fontSize);
    connect(fontSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        updateFontSize(v);
    });
    form->addRow(tr("字号"), fontSpin);

    auto *themeCombo = new QComboBox(&dlg);
    for (const QString &tid : Theme::availableThemes()) {
        themeCombo->addItem(Theme::palette(tid).displayName, tid);
    }
    const int idx = themeCombo->findData(m_themeId);
    if (idx >= 0) {
        themeCombo->setCurrentIndex(idx);
    }
    // context 用 &dlg 而不是 this：连接随对话框的生命周期结束而解除，
    // 不会在对话框销毁后仍指向栈上的 themeCombo。
    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [this, themeCombo](int i) {
        applyThemeById(themeCombo->itemData(i).toString());
    });
    form->addRow(tr("主题"), themeCombo);

    auto *darkBox = new QCheckBox(tr("深色模式"), &dlg);
    darkBox->setChecked(m_darkMode);
    connect(darkBox, &QCheckBox::toggled, this, &MainWindow::setDarkMode);
    form->addRow(QString(), darkBox);

    auto *smartBox = new QCheckBox(tr("智能标点"), &dlg);
    smartBox->setChecked(m_smartPunctuation->isEnabled());
    connect(smartBox, &QCheckBox::toggled, this, &MainWindow::onSmartPunctuationToggled);
    form->addRow(QString(), smartBox);

    auto *typeBox = new QCheckBox(tr("打字机模式"), &dlg);
    typeBox->setChecked(m_typewriterMode);
    connect(typeBox, &QCheckBox::toggled, this, &MainWindow::setTypewriterMode);
    form->addRow(QString(), typeBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    dlg.setLayout(form);
    dlg.exec();
}

void MainWindow::showPrintDialog()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() == QDialog::Accepted) {
        editor->document()->print(&printer);
    }
}

void MainWindow::showDevTools()
{
    // 列出可用的 Qt 调试视图，作为「开发者工具」的轻量替代
    QMenu menu(tr("开发者工具"), this);
    QAction *logAct = menu.addAction(tr("查看输出日志"));
    QAction *objAct = menu.addAction(tr("对象树"));
    QAction *propAct = menu.addAction(tr("属性"));
    QAction *chosen = menu.exec(QCursor::pos());
    if (!chosen) {
        return;
    }
    if (chosen == logAct) {
        QMessageBox::information(this, tr("开发者工具"),
                                 tr("日志输出：\n%1").arg(qInstallMessageHandler(nullptr)
                                     ? QStringLiteral("(已安装)") : QStringLiteral("(无)")));
    } else if (chosen == objAct) {
        QStringList names;
        for (QObject *child : findChildren<QObject*>()) {
            names << QStringLiteral("%1 (%2)")
                          .arg(child->metaObject()->className())
                          .arg(child->objectName());
        }
        QMessageBox::information(this, tr("对象树"),
                                 names.join('\n'));
    } else if (chosen == propAct) {
        QMessageBox::information(this, tr("属性"),
                                 tr("窗口尺寸：%1 x %2\n主题：%3\n字体：%4")
                                     .arg(width()).arg(height())
                                     .arg(m_themeId).arg(m_fontSize));
    }
}

void MainWindow::benchStart()
{
    if (!m_benchMode) {
        return;
    }
    m_benchLog.clear();
    m_benchTimer.start();
    // t=0 基线
    m_benchLog << QStringLiteral("bench_start|0");
}

void MainWindow::benchMark(const QString &phase)
{
    if (!m_benchMode) {
        return;
    }
    const double ms = m_benchTimer.nsecsElapsed() / 1000000.0;
    m_benchLog << QStringLiteral("%1|%2").arg(phase).arg(ms, 0, 'f', 2);
}
