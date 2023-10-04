// PCG random number generator taken from https://en.wikipedia.org/wiki/Permuted_congruential_generator

static u64 state            = 0x4d595df4d0f33173;   // Or something seed-dependent
static u64 const multiplier = 6364136223846793005u;
static u64 const increment  = 1442695040888963407u;	// Or an arbitrary odd constant

static u32 rotr32(u32 x, unsigned r) {
	return x >> r | x << (-r & 31);
}

u32 pcg32(void) {
	u64 x = state;
	unsigned count = (unsigned)(x >> 59);   // 59 = 64 - 5

	state = x * multiplier + increment;
	x ^= x >> 18;                           // 18 = (64 - 27)/2
	return rotr32((u32)(x >> 27), count);   // 27 = 32 - 5
}

void pcg32_init(u64 seed) {
	state = seed + increment;
	(void)pcg32();
}

u32 rand_range_u32(u32 min, u32 max) {
	u32 range = max - min + 1;
	return min + (pcg32() % range);
}

f32 rand_f32(void) {
	return (f32)((f64)pcg32() / (f64)UINT32_MAX);
}
