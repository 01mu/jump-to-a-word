# Jump to a Word

## Introduction
**Jump to a Word** is a plugin for [Geany](https://github.com/geany/geany) that lets the user instantly move their cursor to a visible word, character, substring, or line. It also provides features for more precise text selection and transformation. The development of this plugin was influenced by the GNU Emacs package [avy](https://github.com/abo-abo/avy) and specifically [this demonstration of it](https://www.youtube.com/watch?v=EsAkPl3On3E&t=1333s).

## Overview
<p>The plugin allows the user to:</p>
<ul>
  <li>Use shortcuts to jump to a word, character, or line</li>
  <li>Use a search term to jump to a word or substring</li>
  <li>Edit all occurances of a selected word, character, or substring</li>
  <li>Perform an action or selection after jumping to a word, character, substring, or line</li>
</ul>
<p>These actions can be triggered from the menu, the command panel, or a keybinding.</p>

### Jumping to a word using a shortcut
<p>Places a shortcut on every word on the screen and moves the cursor to that word when pressed.</p>

![Jumping to a word using a shortcut](https://github.com/user-attachments/assets/4e01e950-bec6-4e33-b117-1f7e484495a7)

### Jumping to a character using a shortcut
<p>Places a shortcut on every character on the screen that matches the provided query.</p>

![Jumping to a character using a shortcut](https://github.com/user-attachments/assets/c2a42525-f643-4076-80f1-f90e970f46dc)

### Jumping to a line using a shortcut
<p>Places a shortcut on every line on the screen.</p>

![Jumping to a line using a shortcut](https://github.com/user-attachments/assets/47630cc2-9573-4abe-9ce4-a3cb54754e59)

### Jumping to a word using a search term
<p>Highlights every word on the screen that matches the provided search term. The words can be cycled through using the left and right arrow keys.</p>

![Jumping to a word using a search term](https://github.com/user-attachments/assets/2aa1b6c2-2894-4a70-8b2c-c6ef8b9b940a)

### Jumping to a substring using a search term
<p>Highlights every substring on the screen that matches the provided search term.</p>

![Jumping to a substring using a search term](https://github.com/user-attachments/assets/d7fbeaa0-23c2-4c53-bdf0-626de53da39e)

### Editing all occurances of a selected word, character, or substring
<p>You can replace the selected text by using the "Replace selected text" function during a character shortcut jump, a word search, or a substring search. All occurances of a selected character, the word under the cursor, or a selected substring will be instantly tagged if you are not in shortcut or search mode.</p>

### Performing an action after jumping to a word, character, substring, or line
<p>You can either select the text or select to the position of the text after jumping to a word, character, or substring. You may also select the text contained in the range between two positions. The action that occurs can be changed in the plugin preferences.</p>

### Jumping or searching within a selection
<p>If a range of text is selected, the text within that selection will be used for jumping or searching. This can be disabled in the plugin preferences.<p>

### Jumping to the previous cursor position
<p>You can move the cursor back to its previous position after a jump.</p>

## Building
Make sure you have [Geany](https://github.com/geany/geany) installed:
<br>
<br>
`apt-get install libgtk-3-dev autoconf automake autopoint gettext`
<br>
`cd jump-to-a-word && make`
<br>
<br>
Then move `jump-to-a-word.so` to your plugin path.
