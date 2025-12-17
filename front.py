from PyQt5 import QtWidgets
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QTextEdit, QPushButton, QVBoxLayout,
    QWidget, QHBoxLayout, QFrame, QMessageBox
)
from PyQt5.QtGui import QIcon, QColor
from PyQt5.QtCore import Qt
import sys
import subprocess
import os

# ----------------- Backend Function -----------------
def run_backend(option):
    backend_path = "C:/Users/OM KRISHALI/Desktop/peri/output/backend.exe"  # full path
    if not os.path.exists(backend_path):
        raise FileNotFoundError(f"Backend executable not found at {backend_path}")
    try:
        result = subprocess.run(
            [backend_path],
            input=f"{option}\n",
            text=True,
            capture_output=True,
            timeout=10  # Add timeout to prevent hanging
        )
        if result.returncode != 0:
            if result.stderr:
                raise RuntimeError(f"Backend error: {result.stderr}")
            raise RuntimeError("Backend process failed")
        return result.stdout.strip()
    except subprocess.TimeoutExpired:
        raise RuntimeError("Backend process timed out")
    except Exception as e:
        raise RuntimeError(f"Backend error: {str(e)}")

# ----------------- Frontend Window -----------------
def window():
    app = QApplication(sys.argv)
    win = QMainWindow()
    win.setGeometry(260, 80, 1000, 700)
    win.setWindowTitle("Om's Word Processor")
    # try to load a nicer app icon if available
    icon_path = "D:/programming/mini_word_processor/image.png"
    if os.path.exists(icon_path):
        win.setWindowIcon(QIcon(icon_path))
    else:
        win.setWindowIcon(QIcon())

    central_widget = QWidget()
    win.setCentralWidget(central_widget)

    main_layout = QVBoxLayout()
    central_widget.setLayout(main_layout)

    # --------- Toolbar Frame ----------
    toolbar_frame = QFrame()
    toolbar_frame.setFrameShape(QFrame.StyledPanel)
    toolbar_frame.setStyleSheet("""
        QFrame { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #e6f0ff, stop:1 #d9e6ff); 
                 border-radius: 10px; padding:6px 10px; }
        QPushButton { background: transparent; border: none; padding:8px 12px; font-size:14px; }
        QPushButton#primary { background: #3b82f6; color: white; border-radius:8px; }
        QPushButton#primary:hover { background: #2563eb; }
        QPushButton:hover { background: rgba(0,0,0,0.06); border-radius:6px; }
    """)

    toolbar_layout = QHBoxLayout()
    toolbar_layout.setAlignment(Qt.AlignLeft)
    toolbar_layout.setContentsMargins(10, 6, 10, 6)
    toolbar_layout.setSpacing(10)
    toolbar_frame.setLayout(toolbar_layout)

    # --------- Tabbed Text Area ----------
    tab_widget = QtWidgets.QTabWidget()
    tab_widget.setTabsClosable(True)
    tab_widget.setMovable(True)

    untitled_count = 1

    def create_tab(title=None, content="", filename=None):
        nonlocal untitled_count
        editor = QTextEdit()
        editor.setPlainText(content)
        editor.setPlaceholderText("Start typing here...")
        editor.setStyleSheet("QTextEdit { font-size:16px; padding:12px; }")
        editor.setFontPointSize(12)
        if not title:
            title = f"Untitled {untitled_count}"
        if not filename:
            filename = f"document_{untitled_count}.txt"
        index = tab_widget.addTab(editor, title)
        # attach filename to editor widget for reliable lookup
        try:
            editor._filename = filename
        except Exception:
            pass
        untitled_count += 1
        tab_widget.setCurrentIndex(index)
        return editor

    # start with one tab
    create_tab()

    # --------- Buttons ----------
    # create a compact icon+label button widget
    def make_icon_label(icon_text, label_text, tooltip=None, object_name=None):
        widget = QWidget()
        layout = QVBoxLayout()
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(4)
        btn = QPushButton(icon_text)
        # 3D-ish button styling: gradient, border and pressed effect
        btn.setFlat(False)
        btn.setStyleSheet('''
            QPushButton {
                font-size:20px;
                background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffffff, stop:1 #e6f0ff);
                border: 1px solid #bcd6ff;
                border-radius:12px;
                padding:6px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #f8fbff, stop:1 #dfeeff);
            }
            QPushButton:pressed {
                background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #dfe6ff, stop:1 #c6dbff);
                padding-top:8px;
            }
        ''')
        btn.setFixedSize(60,60)
        btn.setCursor(Qt.PointingHandCursor)
        # add drop shadow for depth
        try:
            effect = QtWidgets.QGraphicsDropShadowEffect()
            effect.setBlurRadius(14)
            effect.setOffset(0, 4)
            effect.setColor(QColor(0, 0, 0, 80))
            btn.setGraphicsEffect(effect)
        except Exception:
            pass

        lbl = QtWidgets.QLabel(label_text)
        lbl.setAlignment(Qt.AlignCenter)
        lbl.setStyleSheet('font-size:11px; color:#283046; font-weight:600;')
        layout.addWidget(btn, alignment=Qt.AlignCenter)
        layout.addWidget(lbl)
        widget.setLayout(layout)
        if tooltip:
            widget.setToolTip(tooltip)
        if object_name:
            widget.setObjectName(object_name)
        # style each icon card: subtle 3D card
        widget.setStyleSheet("""
            QWidget { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffffff, stop:1 #fbfdff); border-radius:12px; }
            QWidget:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #f7fbff, stop:1 #eaf4ff); }
            QLabel { background: transparent; }
        """)
        # expose the button so we can connect signals
        widget.button = btn
        return widget
    def color_button(weight,color):
        weight.button.setStyleSheet(f"""
            QPushButton{{
                font-size:20px;
                background-color:{color};
                border:1px solid black;
                border-radius:12px;
                color:white;
            }}
            QPushButton:hover{{
                background-color:#555;
            }}""")
    btn_new_w = make_icon_label("🆕", "New", "New (push current to undo and clear)", "primary")
    btn_open_w = make_icon_label("📂", "Open", "Open a file from disk")
    btn_save_w = make_icon_label("💾", "Save", "Save current document")
    btn_undo_w = make_icon_label("↩", "Undo", "Undo last character")
    btn_redo_w = make_icon_label("↪", "Redo", "Redo")
    btn_search_w = make_icon_label("🔍", "Search", "Search and highlight word")
    btn_replace_w = make_icon_label("✏", "Replace", "Replace text")
    
    # --------- Add buttons ----------
    # left side buttons
    for btn in [btn_new_w, btn_open_w, btn_save_w]:
        toolbar_layout.addWidget(btn)
    toolbar_layout.addStretch()
    for btn in [btn_undo_w, btn_redo_w, btn_search_w, btn_replace_w]:
        toolbar_layout.addWidget(btn)

    # Enable replace functionality
    btn_replace_w.button.setEnabled(True)
    btn_replace_w.setToolTip("Replace text")
    # Enable redo - we'll use the frontend's undo/redo (QTextEdit) and persist changes to backend
    btn_redo_w.button.setEnabled(True)
    btn_redo_w.setToolTip("Redo (frontend)")

    # keyboard shortcuts
    # shortcuts (use the internal QPushButton inside our widget)
    btn_save_w.button.setShortcut('Ctrl+S')
    btn_open_w.button.setShortcut('Ctrl+O')
    btn_undo_w.button.setShortcut('Ctrl+Z')

    # --------- Button Functions ----------
    def current_editor():
        return tab_widget.currentWidget()

    def current_index():
        return tab_widget.currentIndex()

    def current_filename():
        editor = current_editor()
        if editor and hasattr(editor, '_filename'):
            return editor._filename
        idx = current_index()
        return f"document_{idx}.txt"

    def save_current_text():
        editor = current_editor()
        if not editor:
            return ""
        try:
            content = editor.toPlainText()
            filename = current_filename()
            out = run_backend(f"save:{filename}::{content}")
            status.showMessage(f"Saved {filename}", 2000)
            # update tab text (basename)
            tab_widget.setTabText(current_index(), os.path.basename(filename))
            return out
        except Exception as e:
            QMessageBox.critical(win, "Save Error", f"Failed to save file: {str(e)}")
            return ""

    def open_file():
        path, _ = QtWidgets.QFileDialog.getOpenFileName(win, "Open file", os.getcwd(), "Text Files (*.txt);;All Files (*)")
        if path:
            try:
                with open(path, 'r', encoding='utf-8') as f:
                    text = f.read()
                # create a new tab for the opened file
                name = os.path.basename(path)
                editor = create_tab(title=name, content=text, filename=name)
                # save content to backend under that filename
                run_backend(f"save:{name}::{text}")
            except Exception as e:
                QMessageBox.warning(win, 'Open failed', str(e))

    def new_clicked():
        # create new tab for work
        create_tab()
        status.showMessage("New tab created", 2000)

    def replace_clicked():
        editor = current_editor()
        if not editor:
            return
            
        # Get the old and new words
        old_word, ok = QtWidgets.QInputDialog.getText(win, "Replace", "Enter word to replace:")
        if not ok or not old_word:
            return
            
        new_word, ok = QtWidgets.QInputDialog.getText(win, "Replace With", f"Replace '{old_word}' with:")
        if not ok:
            return
            
        try:
            # First save current content to ensure backend is in sync
            save_current_text()
            
            # Get the current text
            text = editor.toPlainText()
            
            # Replace the text locally first
            new_text = ""
            last_pos = 0
            replacements = 0
            
            # Find all occurrences of the word (case-sensitive)
            for i in range(len(text)):
                if i < last_pos:
                    continue
                if text[i:i+len(old_word)] == old_word:
                    new_text += text[last_pos:i] + new_word
                    last_pos = i + len(old_word)
                    replacements += 1
            
            # Add remaining text
            if last_pos < len(text):
                new_text += text[last_pos:]
            
            if replacements == 0:
                QMessageBox.information(win, "Replace Result", f"No occurrences of '{old_word}' found.")
                return
                
            # Update the editor and save to backend
            editor.setPlainText(new_text)
            save_current_text()
            
            status.showMessage(f"Replaced {replacements} occurrence(s) of '{old_word}' with '{new_word}'", 3000)
            
        except Exception as e:
            QMessageBox.critical(win, "Replace Error", f"Error during replace: {str(e)}")
            return

    def undo_clicked():
        editor = current_editor()
        if not editor:
            return
        # use QTextEdit's built-in undo (groups user edits reasonably)
        editor.undo()
        # persist new content to backend so CURRENT_FILE matches frontend
        content = editor.toPlainText()
        filename = current_filename()
        run_backend(f"save:{filename}::{content}")
        status.showMessage("Undo", 1500)

    def redo_clicked():
        editor = current_editor()
        if not editor:
            return
        editor.redo()
        # persist new content to backend so CURRENT_FILE matches frontend
        content = editor.toPlainText()
        filename = current_filename()
        run_backend(f"save:{filename}::{content}")
        status.showMessage("Redo", 1500)

    def search_clicked():
        word, ok = QtWidgets.QInputDialog.getText(win, "Search Word", "Enter word to search:")
        if ok and word:
            try:
                editor = current_editor()
                if not editor:
                    return
                
                # Save current text to ensure backend is in sync
                save_current_text()
                
                # Get search results from backend
                output = run_backend(f"search:{word}")
                
                if "not found" in output.lower():
                    msg = QMessageBox()
                    msg.setIcon(QMessageBox.Information)
                    msg.setWindowTitle("Search Result")
                    msg.setText(f"'{word}' not found in document.")
                    msg.setStandardButtons(QMessageBox.Ok)
                    msg.exec_()
                else:
                    # Get the current text and format it as HTML
                    text = editor.toPlainText()
                    formatted_text = ""
                    last_pos = 0
                    
                    # Find all occurrences of the word (case-insensitive)
                    for i in range(len(text)):
                        if i < last_pos:
                            continue
                        if text[i:i+len(word)].lower() == word.lower():
                            # Add text before match
                            formatted_text += text[last_pos:i].replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
                            # Add highlighted match
                            formatted_text += f'<span style="background-color: yellow;">{text[i:i+len(word)]}</span>'
                            last_pos = i + len(word)
                    
                    # Add remaining text
                    if last_pos < len(text):
                        formatted_text += text[last_pos:].replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
                    
                    # Set the HTML content
                    editor.setHtml(formatted_text)
                    status.showMessage(f"Found matches for '{word}'", 2000)
            except Exception as e:
                QMessageBox.critical(win, "Search Error", f"Error during search: {str(e)}")
                return

    # connect buttons
    # connect signals from inner buttons
    btn_new_w.button.clicked.connect(new_clicked)
    btn_open_w.button.clicked.connect(open_file)
    btn_save_w.button.clicked.connect(save_current_text)
    btn_replace_w.button.clicked.connect(replace_clicked)
    btn_undo_w.button.clicked.connect(undo_clicked)
    btn_redo_w.button.clicked.connect(redo_clicked)
    btn_search_w.button.clicked.connect(search_clicked)

    # remove tab handler: keep tab_info consistent
    def on_tab_close(index):
        # remove tab; filename is stored on widget so just remove
        tab_widget.removeTab(index)

    tab_widget.tabCloseRequested.connect(on_tab_close)

    # status bar
    status = win.statusBar()
    status.showMessage('Ready')

    # --------- Add to main layout ----------
    main_layout.addWidget(toolbar_frame)
    main_layout.addWidget(tab_widget)

    win.show()
    sys.exit(app.exec_())

window()