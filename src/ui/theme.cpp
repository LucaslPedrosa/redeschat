#include "ui/theme.hpp"

QString Theme::styleSheet()
{
    return R"(
        QMainWindow {
            background-color: #11110f;
        }
        QWidget {
            background-color: transparent;
            color: #e8e6df;
            font-family: "Inter", "Noto Sans", sans-serif;
            font-size: 13px;
        }
        QFrame#card, QFrame#panel, QFrame#sidebar {
            background-color: #181815;
            border: 1px solid #2b2a25;
            border-radius: 8px;
        }
        QFrame#sidebar {
            background-color: #151512;
        }
        QFrame#headerBar, QFrame#composer {
            background-color: #181815;
            border: 1px solid #2b2a25;
            border-radius: 7px;
        }
        QLabel#brand, QLabel#sectionTitle {
            color: #d7a44a;
            font-size: 11px;
            font-weight: 700;
            letter-spacing: 1.5px;
        }
        QLabel#pageTitle {
            color: #f3f0e7;
            font-size: 25px;
            font-weight: 650;
        }
        QLabel#chatTitle {
            color: #f3f0e7;
            font-size: 17px;
            font-weight: 650;
        }
        QLabel#description, QLabel#hint, QLabel#muted {
            color: #96938a;
        }
        QLabel#statusPill {
            background-color: #20251d;
            border: 1px solid #3f5135;
            border-radius: 10px;
            color: #a9c48f;
            font-size: 11px;
            padding: 3px 9px;
        }
        QPushButton {
            background-color: #c6933c;
            border: 1px solid #d7a44a;
            border-radius: 5px;
            color: #17130c;
            min-height: 32px;
            padding: 0 14px;
            font-weight: 650;
        }
        QPushButton:hover {
            background-color: #d7a44a;
        }
        QPushButton:pressed {
            background-color: #af7e2f;
        }
        QPushButton:disabled {
            background-color: #26251f;
            border-color: #34332d;
            color: #69675f;
        }
        QPushButton#secondaryButton, QPushButton#compactButton {
            background-color: transparent;
            border: 1px solid #3a3932;
            color: #c9c6bc;
        }
        QPushButton#secondaryButton:hover, QPushButton#compactButton:hover {
            background-color: #24231e;
            border-color: #555248;
            color: #f0ede4;
        }
        QPushButton#compactButton {
            min-height: 27px;
            padding: 0 9px;
            font-size: 12px;
        }
        QLineEdit, QSpinBox, QComboBox {
            background-color: #0e0e0c;
            border: 1px solid #35342e;
            border-radius: 5px;
            color: #e8e6df;
            min-height: 32px;
            padding: 0 9px;
            selection-background-color: #765a29;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #b88736;
        }
        QComboBox::drop-down {
            border: none;
            width: 22px;
        }
        QComboBox QAbstractItemView {
            background-color: #181815;
            border: 1px solid #3a3932;
            selection-background-color: #373126;
            selection-color: #f3f0e7;
        }
        QTextEdit {
            background-color: #11110f;
            border: none;
            color: #d7d4ca;
            padding: 12px;
            font-family: "JetBrains Mono", "Noto Sans Mono", monospace;
            selection-background-color: #765a29;
        }
        QTextEdit#serverLog {
            background-color: #0e0e0c;
            border: 1px solid #2b2a25;
            border-radius: 6px;
        }
        QFormLayout QLabel {
            color: #aaa79e;
        }
        QListWidget {
            background-color: transparent;
            border: none;
            outline: none;
            padding: 0;
        }
        QListWidget::item {
            border-radius: 4px;
            color: #aaa79e;
            min-height: 26px;
            padding: 2px 7px;
        }
        QListWidget::item:hover {
            background-color: #22211c;
            color: #e8e6df;
        }
        QListWidget::item:selected {
            background-color: #373126;
            color: #e7be77;
        }
        QTabWidget::pane {
            background-color: #11110f;
            border: 1px solid #2b2a25;
            border-radius: 6px;
            top: -1px;
        }
        QTabBar::tab {
            background-color: transparent;
            border: none;
            border-bottom: 2px solid transparent;
            color: #858279;
            min-width: 80px;
            padding: 8px 13px;
        }
        QTabBar::tab:hover {
            color: #d2cfc5;
        }
        QTabBar::tab:selected {
            border-bottom-color: #d7a44a;
            color: #e7be77;
        }
        QProgressBar {
            background-color: #22211c;
            border: none;
            border-radius: 3px;
            color: #e8e6df;
            max-height: 16px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #b88736;
            border-radius: 3px;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 9px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: #3a3932;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QToolTip {
            background-color: #24231e;
            border: 1px solid #48463e;
            color: #e8e6df;
            padding: 5px;
        }
    )";
}
