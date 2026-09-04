// Nova language definition for Monaco. Keyword list is kept in sync with
// lexer.h's TOKEN_TYPE_LIST by hand — if you add a keyword there, add it
// here too (keywords, keywordText nullptr entries are operators/literals
// and don't belong in this list).
function registerNovaLanguage(monaco) {
  monaco.languages.register({ id: 'nova' });

  monaco.languages.setMonarchTokensProvider('nova', {
    keywords: [
      'quest', 'loot', 'relic', 'skill', 'reward', 'when', 'otherwise',
      'grind', 'character', 'spawn', 'nothing', 'curse', 'finish',
      'break', 'continue', 'and', 'or', 'equip',
    ],
    // Not real keywords (they're plain identifiers the compiler special-cases
    // in codegen), but worth highlighting distinctly so a script reads well.
    builtins: [
      'announce', 'array_new', 'length', 'free_array', 'free_string',
      'init_window', 'set_fps', 'draw_rect', 'clear_screen', 'render_frame',
      'is_key_down', 'random', 'close_window',
    ],
    operators: [
      '+', '-', '*', '/', '//', '%', '=', '==', '!=',
      '<', '<=', '>', '>=', '!', '+=', '-=', '*=', '/=',
    ],
    symbols: /[=><!+\-*/%]+/,

    tokenizer: {
      root: [
        [/#.*$/, 'comment'],
        [/\/\*/, 'comment', '@blockComment'],

        [/[a-zA-Z_]\w*/, {
          cases: {
            '@keywords': 'keyword',
            '@builtins': 'predefined',
            '@default': 'identifier',
          },
        }],

        [/\d+\.\d+|\d+/, 'number'],
        [/"/, 'string', '@string'],

        [/[()[\]]/, '@brackets'],
        [/,/, 'delimiter'],
        [/@symbols/, {
          cases: {
            '@operators': 'operator',
            '@default': '',
          },
        }],

        [/\s+/, 'white'],
      ],

      blockComment: [
        [/[^*]+/, 'comment'],
        [/\*\//, 'comment', '@pop'],
        [/./, 'comment'],
      ],

      string: [
        [/[^"]+/, 'string'],
        [/"/, 'string', '@pop'],
      ],
    },
  });

  monaco.languages.setLanguageConfiguration('nova', {
    comments: { lineComment: '#', blockComment: ['/*', '*/'] },
    brackets: [['(', ')'], ['[', ']']],
    autoClosingPairs: [
      { open: '(', close: ')' },
      { open: '[', close: ']' },
      { open: '"', close: '"' },
    ],
  });

  monaco.editor.defineTheme('nova-ink', {
    base: 'vs-dark',
    inherit: true,
    rules: [
      { token: 'keyword', foreground: 'd9a441', fontStyle: 'bold' },
      { token: 'predefined', foreground: '7fb8ab' },
      { token: 'comment', foreground: '6b6a80', fontStyle: 'italic' },
      { token: 'string', foreground: 'b8c98a' },
      { token: 'number', foreground: 'c98a6f' },
      { token: 'operator', foreground: 'ede8dd' },
    ],
    colors: {
      'editor.background': '#1a1c2b',
      'editor.foreground': '#ede8dd',
      'editorLineNumber.foreground': '#4a4c63',
      'editorLineNumber.activeForeground': '#8b8a9e',
      'editor.selectionBackground': '#2c2e4280',
      'editorCursor.foreground': '#d9a441',
      'editor.lineHighlightBackground': '#20222f',
      'editorIndentGuide.background': '#2c2e42',
    },
  });
}
