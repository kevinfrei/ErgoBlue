// Some stuff to make the action maps prettier. I use Clang Format, and it
// messes up the comments if they're over 80 characters on any individual
// line...
#define LCMD MOD(LEFTCOMMAND)
#define LSHFT MOD(LEFTSHIFT)
#define LCTL MOD(LEFTCTRL)
#define LOPT MOD(LEFTOPTION)
#define RCMD MOD(RIGHTCOMMAND)
#define RSHFT MOD(LEFTSHIFT)
#define RCTL MOD(RIGHTSHIFT)
#define ROPT MOD(RIGHTOPTION)

#define GRV GRAVE
#define OBRC BRACKET_LEFT
#define CBRC BRACKET_RIGHT
#define BKSP BACKSPACE
#define DEL DELETE
#define PGUP PAGE_UP
#define PGDN PAGE_DOWN
#define SEMI_ KEY(SEMICOLON)
#define COMMA_ KEY(COMMA)
#define DOT_ KEY(PERIOD)
#define QUOTE_ KEY(APOSTROPHE)
#define ENTER_ KEY(RETURN)
#define UP_ KEY(ARROW_UP)
#define DOWN_ KEY(ARROW_DOWN)
#define LEFT_ KEY(ARROW_LEFT)
#define RIGHT_ KEY(ARROW_RIGHT)
#define SPACE_ KEY(SPACE)
#define M_MUTE KEY(M_MUTE)
#define M_VOLU KEY(M_VOLUME_UP)
#define M_VOLD KEY(M_VOLUME_DOWN)

#define M_PLAY CONS(M_PLAY)
#define M_PREV CONS(M_PREVIOUS_TRACK)
#define M_NEXT CONS(M_NEXT_TRACK)

const action_t keymap[][numcols * numrows * 2] = {
    {LROW1(KEY(ESCAPE), KEY(1), KEY(2), KEY(3), KEY(4), KEY(5), M_VOLD),
     LROW2(KEY(TAB), KEY(Q), KEY(W), KEY(E), KEY(R), KEY(T), M_PLAY),
     LROW3(LCMD, KEY(A), KEY(S), KEY(D), KEY(F), KEY(G), KEY(GRAVE), LCMD),
     LROW4(LSHFT, KEY(Z), KEY(X), KEY(C), KEY(V), KEY(B), M_PREV, KEY(HOME)),
     LROW5(LCTL, LOPT, LCMD, M_MUTE, KEY(OBRC), KEY(BKSP), KEY(DEL), KEY(END)),

     RROW1(M_VOLU, KEY(6), KEY(7), KEY(8), KEY(9), KEY(0), KEY(MINUS)),
     RROW2(ROPT, KEY(Y), KEY(U), KEY(I), KEY(O), KEY(P), KEY(BACKSLASH)),
     RROW3(RCMD, KEY(EQUAL), KEY(H), KEY(J), KEY(K), KEY(L), SEMI_, QUOTE_),
     RROW4(KEY(PGUP), M_NEXT, KEY(N), KEY(M), COMMA_, DOT_, KEY(SLASH), RSHFT),
     RROW5(KEY(PGDN), ENTER_, SPACE_, KEY(CBRC), LEFT_, UP_, DOWN_, RIGHT_)}};
