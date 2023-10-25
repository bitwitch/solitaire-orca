#define ARRAY_COUNT(array) (sizeof(array) / sizeof(*(array)))
#define CARD_ASPECT (560.0f/780.0f)
#define STOCK_OFFSET_BETWEEN_CARDS 0.5
#define MAX_DIST_CONSIDERED_CLICK 2.0f 
#define GRAVITY 2000.0f

typedef enum {
	PILE_NONE,
	PILE_STOCK,
	PILE_WASTE,
	PILE_FOUNDATION,
	PILE_TABLEAU,
} PileKind;

typedef struct {
	PileKind kind;
	oc_vec2 pos;
	oc_list cards;
} Pile;

typedef enum {
	CARD_ACE,
	CARD_TWO,
	CARD_THREE,
	CARD_FOUR,
	CARD_FIVE,
	CARD_SIX,
	CARD_SEVEN,
	CARD_EIGHT,
	CARD_NINE,
	CARD_TEN,
	CARD_JACK,
	CARD_QUEEN,
	CARD_KING,
	CARD_KIND_COUNT,
} CardKind;

typedef enum {
	SUIT_CLUB,
	SUIT_DIAMOND,
	SUIT_HEART,
	SUIT_SPADE,
	SUIT_COUNT
} Suit;

typedef struct {
	Suit suit;
	CardKind kind;
	oc_vec2 pos;
} CardPath;

typedef struct {
	Suit suit;
	CardKind kind;
	Pile *pile;
	oc_vec2 pos;
	oc_vec2 vel; // for win animation
	oc_vec2 target_pos;
	oc_vec2 drag_offset; // offset from mouse to top left corner of card
	oc_vec2 pos_before_drag;
	bool face_up;
	oc_list_elt node;
} Card;

typedef struct {
	bool down, was_down;
} DigitalInput;

typedef struct {
    float x;
    float y;
    float deltaX;
    float deltaY;
	DigitalInput left, right;
} MouseInput;

typedef struct {
	DigitalInput r, u;
	DigitalInput num1, num2, num3, num4, num5, num6, num7, num8, num9, num0;
} Input;

typedef enum {
	UNDO_NONE,
	UNDO_SCORE_CHANGE,
	UNDO_PILE_TRANSFER,
	UNDO_COMMIT_MARKER,
} UndoKind;

typedef struct {
	UndoKind kind;
	union {
		struct {
			Pile *prev_pile;
			Card *card, *parent;
			bool was_face_up, was_parent_face_up;
		};
		i32 score_change;
	};
} UndoInfo;

typedef enum {
	SCORE_NONE,
	SCORE_RESET,
	SCORE_PILE_TRANSFER,
	SCORE_REVEAL_TABLEAU,
	SCORE_RECYCLE_WASTE,
	SCORE_UNDO,
	SCORE_TIME_BONUS,
} ScoreKind;

typedef struct {
	ScoreKind kind;
	union {
		struct {
			Pile *from_pile;
			Pile *to_pile;
		};
		i32 score_change;
	};
} UpdateScoreParams;

typedef enum {
	STATE_NONE,
	STATE_DEALING,
	STATE_PLAY,
	STATE_SHOW_RULES,
	STATE_SELECT_CARD_BACK,
	STATE_AUTOCOMPLETE,
	STATE_WIN,
} StateKind;

typedef struct {
	StateKind state, restore_state;
	bool draw_three_mode;
	oc_surface surface;
	oc_canvas canvas;
	oc_ui_context ui;
	oc_font font;
	oc_color bg_color;

	MouseInput mouse_input;
	Input input;
	oc_vec2 mouse_pos_on_mouse_right_down;

	f64 dt, last_timestamp, timer;
	char timer_string[9]; // 00:00:00
	f32 deal_speed;
	f32 card_animate_speed;

	f64 deal_countdown, deal_delay;
	i32 deal_tableau_index;     // used for calculating 
	i32 deal_tableau_remaining; // where to deal cards
	i32 deal_cards_remaining;

	oc_color menu_bg_color;
	bool menu_opened;
	i32 menu_card_backs_margin;
	oc_ui_box *menu_card_backs_draw_box;

	i32 win_foundation_index;
	Card *win_moving_card;
	i32 win_card_path_index;

	i32 temp_undo_stack_index;
	i32 undo_stack_index;
	i32 move_count;
	i32 undo_count;
	char moves_string[12]; // Moves: 0000

	i32 score;
	char score_string[14]; // Score: 000000

	oc_vec2 frame_size;
	oc_vec2 board_margin;
	u32 card_width, card_height;
	u32 card_margin_x;
	u32 tableau_margin_top;

	Pile stock, waste;
	Pile foundations[4];
	Pile tableau[7];

	Card *card_dragging;
	
	oc_image spritesheet, reload_icon, rules_images[2], card_backs[10];
	u32 selected_card_back;

	oc_rect card_sprite_rects[SUIT_COUNT][CARD_KIND_COUNT]; 
	Card cards[SUIT_COUNT*CARD_KIND_COUNT];

	CardPath win_card_path[10000];

	UndoInfo temp_undo_stack[64];
	UndoInfo undo_stack[4096];
} GameState;

GameState game;

static oc_ui_sig oc_ui_menu_button_fixed_width(const char* name, f32 width) {
    oc_ui_context* ui = oc_ui_get_context();
    oc_ui_theme* theme = ui->theme;

    oc_ui_style_next(&(oc_ui_style){ .size.width = { OC_UI_SIZE_PIXELS, width },
                                     .size.height = { OC_UI_SIZE_TEXT },
                                     .layout.margin.x = 8,
                                     .layout.margin.y = 4,
                                     .bgColor = { 0, 0, 0, 0 } },
                     OC_UI_STYLE_SIZE
                         | OC_UI_STYLE_LAYOUT_MARGINS
                         | OC_UI_STYLE_BG_COLOR);

    oc_ui_pattern hoverPattern = { 0 };
    oc_ui_pattern_push(&ui->frameArena, &hoverPattern, (oc_ui_selector){ .kind = OC_UI_SEL_STATUS, .status = OC_UI_HOVER });
    oc_ui_style hoverStyle = { .bgColor = theme->fill0 };
    oc_ui_style_match_before(hoverPattern, &hoverStyle, OC_UI_STYLE_BG_COLOR);

    oc_ui_pattern activePattern = { 0 };
    oc_ui_pattern_push(&ui->frameArena, &activePattern, (oc_ui_selector){ .kind = OC_UI_SEL_STATUS, .status = OC_UI_ACTIVE });
    oc_ui_style activeStyle = { .bgColor = theme->fill2 };
    oc_ui_style_match_before(activePattern, &activeStyle, OC_UI_STYLE_BG_COLOR);

    oc_ui_flags flags = OC_UI_FLAG_CLICKABLE
                      | OC_UI_FLAG_CLIP
                      | OC_UI_FLAG_DRAW_TEXT
                      | OC_UI_FLAG_DRAW_BACKGROUND;

    oc_ui_box* box = oc_ui_box_make(name, flags);
    oc_ui_sig sig = oc_ui_box_sig(box);
    return (sig);
}


