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
	DigitalInput r;
	DigitalInput num1, num2, num3, num4, num5;
} Input;

typedef enum {
	STATE_NONE,
	STATE_DEALING,
	STATE_PLAY,
	STATE_AUTOCOMPLETE,
	STATE_WIN,
} StateKind;

typedef struct {
	StateKind state;
	oc_surface surface;
	oc_canvas canvas;
	oc_font font;
	MouseInput mouse_input;
	Input input;
	oc_vec2 mouse_pos_on_mouse_right_down;

	f64 last_timestamp;
	f64 dt;
	f32 deal_speed;
	f32 card_animate_speed;

	f64 deal_countdown, deal_delay;
	i32 deal_tableau_index;     // used for calculating 
	i32 deal_tableau_remaining; // where to deal cards
	i32 deal_cards_remaining;

	i32 win_foundation_index;
	Card *win_moving_card;

	oc_vec2 frame_size;
	oc_vec2 board_margin;
	u32 card_width, card_height;
	u32 card_margin_x;
	u32 tableau_margin_top;

	Pile stock, waste;
	Pile foundations[4];
	Pile tableau[7];

	Card *card_dragging;
	
	oc_image spritesheet, card_backs[5];
	u32 selected_card_back;
	oc_rect card_sprite_rects[SUIT_COUNT][CARD_KIND_COUNT]; 
	Card cards[SUIT_COUNT*CARD_KIND_COUNT];

} GameState;

GameState game;

