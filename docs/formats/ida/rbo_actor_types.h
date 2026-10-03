struct RboAtRecord;
struct RboCharContext;
struct RboActor;
struct RboActor {
 unsigned char unmapped_000[0xC]; // +0x000 not yet traced
 RboActor * nextSibling; // +0x00C Actor_DrawTree 0x4483E0 walks v2+12
 unsigned char unmapped_010[0x4]; // +0x010 not yet traced
 RboActor * firstChild; // +0x014 Actor_DrawTree recurses into v2+20
 unsigned char unmapped_018[0x24]; // +0x018 not yet traced
 int playerId; // +0x03C Actor_Init arg a3; compared in the hit resolver sub_4433E0 (v3[15])
 unsigned char unmapped_040[0x1C]; // +0x040 not yet traced
 unsigned char pattern; // +0x05C Actor_CacheCurrentFrame 0x440920
 unsigned char frame; // +0x05D frame number inside the pattern
 unsigned char unmapped_05E[0x2]; // +0x05E not yet traced
 struct RboFrameRecord * nextFrame; // +0x060 Actor_ResolveNextFrameRecord 0x440970
 unsigned __int16 nextSpriteId; // +0x064 Actor_GetFrameSpriteId result
 unsigned char unmapped_066[0x2]; // +0x066 not yet traced
 int frameTicks; // +0x068 compared with frame.duration in Actor_TickFrame 0x43F830
 unsigned char loopCounter; // +0x06C loaded from frame.loopCount in Actor_EnterFrame 0x43EF30
 unsigned char unmapped_06D[0x33]; // +0x06D not yet traced
 int posX; // +0x0A0 sub_440D50 box placement
 int posY; // +0x0A4 
 int posZ; // +0x0A8 
 unsigned char unmapped_0AC[0x208]; // +0x0AC not yet traced
 int drawDepthOffset; // +0x2B4 sub_440DF0 (draw priority codes 12..61)
 int drawLayer; // +0x2B8 sub_440DF0 (codes 1..11); Actor_DrawTree selects actors by layer
 unsigned char unmapped_2BC[0x2C]; // +0x2BC not yet traced
 int blendMode; // +0x2E8 Actor_EnterFrame copies frame.blendMode here
 unsigned char unmapped_2EC[0x238]; // +0x2EC not yet traced
 int jumpCounter; // +0x524 Actor_TickFrame increments on pattern change
 unsigned __int16 pendingPattern; // +0x528 Actor_ApplyLandJump/hit routines; consumed by Actor_TickFrame, 0xFFFF = none
 unsigned __int16 pendingFrame; // +0x52A 0xFFFF = none
 unsigned char pendingFlip; // +0x52C Actor_TickFrame, 0xFF = none
 unsigned char unmapped_52D[0x3]; // +0x52D not yet traced
 int pendingFlipNegatesVelocity; // +0x530 Actor_TickFrame
 unsigned char facingLeft; // +0x534 Actor_FrameMoveAddSpeed 0x44B3D0
 unsigned char unmapped_535[0x3]; // +0x535 not yet traced
 unsigned char * facingLeftPtr; // +0x538 Actor_Init: points at +1332
 unsigned char unmapped_53C[0x24]; // +0x53C not yet traced
 int altDrawMode; // +0x560 Actor_CacheCurrentFrame: pattern flag 0x40
 struct RboFrameRecord * curFrame; // +0x564 Actor_CacheCurrentFrame
 unsigned char unmapped_568[0x1E4]; // +0x568 not yet traced
 int attackBoxCount; // +0x74C a1[467] Actor_CollectAttackBoxes2 0x440F40
 struct RboAtRecord * curAttack; // +0x750 a1[468]: 120-byte AT record
 void * attackBoxRect[2]; // +0x754 a1[469..470] pointers into the box section
 int attackBoxSlot[2]; // +0x75C a1[471..472] index of the slot in frame.attackBoxIdx
 unsigned char unmapped_764[0xC]; // +0x764 not yet traced
 int hurtBoxCount; // +0x770 a1[476] Actor_CollectHurtBoxes3 0x441120
 void * hurtBoxRect[3]; // +0x774 a1[477..479]
 int hurtBoxSlot[3]; // +0x780 a1[480..482]
 unsigned char unmapped_78C[0x4C]; // +0x78C not yet traced
 int kasanariBoxCount; // +0x7D8 a1[502] Actor_CollectBoxes2_272 0x4411E0
 void * kasanariBoxRect[2]; // +0x7DC a1[503..504]
 int kasanariBoxSlot[2]; // +0x7E4 a1[505..506]
 struct RboCharContext * charContext; // +0x7EC Actor_Init
 struct RboPatternAreaView * patternArea; // +0x7F0 Actor_Init: charContext+28
 unsigned char unmapped_7F4[0x24]; // +0x7F4 not yet traced
 int stanceClass; // +0x818 Actor_ApplyFrameStatusFlags 0x441330 copies frame.stanceClass
 unsigned int hitClassMask; // +0x81C Actor_ApplyFrameStatusFlags: 895 minus frame.hurtMaskFlags classes
 unsigned char unmapped_820[0x110]; // +0x820 not yet traced
 int ticksUntilWrap; // +0x930 Actor_TickFrame: += 256 when <= 0
 unsigned char unmapped_934[0x10]; // +0x934 not yet traced
 unsigned int enterFlags; // +0x944 Actor_EnterFrame sets 7
};
