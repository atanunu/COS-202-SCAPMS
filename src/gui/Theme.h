#pragma once
namespace clinic::gui {
inline const char *theme = R"(
QMainWindow,QDialog { background:#f5f7fa; color:#243849; }
QWidget { font-family:'Segoe UI'; font-size:10pt; color:#243849; }
QLabel#pageTitle { font-size:25pt; font-weight:650; color:#102e37; }
QLabel#sectionTitle { font-size:16pt; font-weight:600; }
QLabel#caption { color:#657781; }
QLabel#metric { font-size:29pt; font-weight:650; color:#125c55; }
QLabel#brand { color:white; font-size:20pt; font-weight:700; }
QLabel#brandCaption { color:#adc9c4; font-size:9pt; }
QFrame#sidebar { background:#103e3b; }
QListWidget { background:white; color:#243849; border:1px solid #cbd8dc; selection-background-color:#d9eeea; selection-color:#133e39; }
QListWidget#navigation { background:transparent; border:0; color:#cee1dd; outline:0; }
QListWidget#navigation::item { padding:15px 14px; margin:3px 0; border-radius:7px; }
QListWidget#navigation::item:selected { background:#26635d; color:white; }
QListWidget#navigation::item:hover { background:#20524d; }
QFrame#card { background:white; border:1px solid #dce6e8; border-radius:10px; }
QPushButton { background:white; border:1px solid #cedadd; border-radius:6px; padding:7px 15px; font-weight:600; }
QPushButton:hover { background:#e8f2f0; border-color:#719d95; }
QPushButton[primary="true"] { background:#147368; border-color:#147368; color:white; }
QPushButton[primary="true"]:hover { background:#105d54; }
QPushButton:disabled { color:#98a7aa; background:#edf0f2; border-color:#e0e5e7; }
QLineEdit,QSpinBox,QComboBox,QDateTimeEdit,QDateEdit,QTextEdit {
background:white; border:1px solid #cbd8dc; border-radius:5px; padding:7px; min-height:22px; }
QLineEdit:focus,QComboBox:focus { border:1px solid #147368; }
QTableView { background:white; alternate-background-color:#f7fafb; border:1px solid #dce6e8; border-radius:7px;
selection-background-color:#d9eeea; selection-color:#133e39; }
QHeaderView::section { background:#edf3f5; color:#536b76; border:0; border-bottom:1px solid #dce6e8; padding:11px 6px; font-weight:600; }
QMenu { background:white; color:#243849; border:1px solid #cbd8dc; }
QMenu::item:selected { background:#d9eeea; color:#133e39; }
QTabBar::tab { background:#edf3f5; color:#243849; padding:7px 12px; border:1px solid #cbd8dc; }
QTabBar::tab:selected { background:white; color:#103e3b; }
QTabWidget::pane { border:1px solid #dce6e8; background:white; }
QComboBox QAbstractItemView { background:white; color:#243849; selection-background-color:#d9eeea; selection-color:#133e39; }
QCalendarWidget QToolButton { color:white; background:#147368; }
QStatusBar { background:white; color:#657781; }
QToolBar { background:white; border-bottom:1px solid #dce6e8; spacing:12px; padding:10px; }
)";
}
