#ifndef UI_VIEW_RENDER_IMPL_H_GUARD
#define UI_VIEW_RENDER_IMPL_H_GUARD

// ============================================================================
// Per-view scene rendering (docs/HANTEI_WAVE2.md §2-§4)
// ============================================================================
// Included from main_frame.cpp.
//
// Every character view renders into its own RenderTarget with its own camera
// (CharacterView::camera()). A pass is fully described by Render::PassParams
// plus SceneOptions; nothing is borrowed from, or written back to, another
// view. The main window shows the main host's active view by blitting its
// target to the back buffer; detached windows show their active view's target
// through ImGui::Image (ui/workspace_hosts_impl.h); PNG export renders into a
// private target (ui/png_export_impl.h).
//
// Spawned actors, onion-skin samples and export ticks all come from the view's
// cached preview simulator (preview_sim.h, getStateAt).

#include <chrono>
#include <climits>
#include "../parts/part_render_math.h"
#include "../startup_args.h"
#include "../opponent_scene.h"
#include "../opponent_release.h"
#include "../extension_profile.h"

namespace viewrender {

static bool SameOpponentPath(std::string a,std::string b) {
	auto normalize=[](std::string& s) { for(char& c:s) { if(c=='\\') c='/'; if(c>='A' && c<='Z') c=char(c-'A'+'a'); } };
	normalize(a); normalize(b); return a==b;
}

// How one actor's frame is placed and coloured.
struct ActorStyle {
	int positionKind=-1, positionIndex=-1;
	float pivotX = 0.f, pivotY = 0.f;
	bool mbaacc = false;
	int tracePattern = -1, traceFrame = -1;
	float offsetX = 0.f, offsetY = 0.f; // world offset from the root actor
	bool mirrored = false;               // facing differs from the root
	float angleTurns = 0.f;              // actor angle (1 = 360 degrees)
	int zPriority = INT_MIN;             // INT_MIN: the frame's AF priority
	int pups = 0;                        // PUPS of the actor's pattern: <cg>_pN.pal (UNI2/MBTL, #76)
	float alphaMul = 1.f;
	glm::vec3 tint{1.f, 1.f, 1.f};
	bool boxes = true;                   // attach the frame's hitboxes to layer 0
	bool spawned = false;
	int flagset1 = 0, flagset2 = 0;
	int layer0Sprite = INT_MIN;          // editor override for layer 0's sprite
	bool patBoxesHalf = false;           // FrameData::patBoxesHalfScale() of the actor's data
	// AFHK: where the actor is inside its frame. `seq` + `frameIndex` locate the frame the game interpolates
	// toward; ticksInFrame is 0 on the frame's first tick (no visible blend yet).
	const Sequence* seq = nullptr;
	int frameIndex = -1;
	int ticksInFrame = 0;
	int loopCounter = 0;
	bool uniDialect = false;             // UNI/UNI2/MBTL colour lerp rules (else MBAACC)
	bool mbtl = false;                   // MBTL does not read PRAS
	CG* tintCg = nullptr;                // PPPA tint palette owner (effect.ha6 actors: the character)
};

// AFHK target of `frameIndex` in `seq` and the current frame's weight (1 = no interpolation).
static const Frame* InterpTarget(const ActorStyle& s, const Frame& frame, float& curWeight)
{
	curWeight = 1.f;
	const auto& af = frame.AF;
	if (!s.seq || s.frameIndex < 0 || af.interpolationType < 1 || af.interpolationType > 5) return nullptr;
	const int next = partrender::NextFrameIndex(s.frameIndex, af.aniType, af.aniFlag, af.jump, af.loopEnd, s.loopCounter,
		partrender::LoopEndCounter(s.uniDialect));
	if (next == s.frameIndex || next < 0 || next >= (int)s.seq->frames.size()) return nullptr;
	curWeight = partrender::CurrentWeight(af.interpolationType, s.ticksInFrame, af.duration, !s.uniDialect);
	return curWeight < 1.f ? &s.seq->frames[next] : nullptr;
}

// Push every AF layer of `frame` (UNI multi-layer AFGX) as render layers.
static void AddFrameLayers(Render& render, Frame& frame, CG* cg, Parts* parts, const ActorStyle& s)
{
	if (frame.AF.layers.empty())
		frame.AF.layers.push_back({});   // MBAACC frames have one implicit layer
	float t = 1.f;
	const Frame* nextFrame = InterpTarget(s, frame, t);
	for (size_t li = 0; li < frame.AF.layers.size(); ++li) {
		if (li < 32 && !((gStartup.layerMask >> li) & 1u)) continue;   // --layer / batch `layer` filter
		const auto& L = frame.AF.layers[li];
		// The same layer of the next frame (AFHK); its offset is never interpolated, only colour,
		// scale and rotation (Han6Draw_BuildLayerMatrix_LerpScaleRot / Han6Draw_DrawLayer).
		const Layer<>* N = (nextFrame && li < nextFrame->AF.layers.size()) ? &nextFrame->AF.layers[li] : nullptr;
		RenderLayer layer;
		layer.positionKind=s.positionKind; layer.positionIndex=s.positionKind==0?int(li):s.positionIndex;
		layer.tracePattern = s.tracePattern; layer.traceFrame = s.traceFrame;
		layer.spriteId = (li == 0 && s.layer0Sprite != INT_MIN) ? s.layer0Sprite : L.spriteId;
		layer.spawnOffsetX = (int)std::lround(s.offsetX);
		layer.spawnOffsetY = (int)std::lround(s.offsetY);
		layer.frameOffsetX = L.offset_x;
		layer.frameOffsetY = L.offset_y;
		layer.scaleX = L.scale[0];
		layer.scaleY = L.scale[1];
		layer.rotX = L.rotation[0];
		layer.rotY = L.rotation[1];
		layer.rotZ = L.rotation[2];
		// The editor saves "no AFAL mode but alpha < 1" as mode 1 (framedata_save.cpp), so that is what the
		// game will see; a layer with no AFAL at all draws at full alpha.
		const int curMode = L.blend_mode ? L.blend_mode : (L.rgba[3] != 1.f ? 1 : 0);
		partrender::LayerRgba rgba{L.rgba[0], L.rgba[1], L.rgba[2], L.rgba[3]};
		if (N) {
			layer.scaleX = partrender::Lerp(L.scale[0], N->scale[0], t);
			layer.scaleY = partrender::Lerp(L.scale[1], N->scale[1], t);
			layer.rotX = partrender::LerpTurns(L.rotation[0], N->rotation[0], t);
			layer.rotY = partrender::LerpTurns(L.rotation[1], N->rotation[1], t);
			layer.rotZ = partrender::LerpTurns(L.rotation[2], N->rotation[2], t);
			const int nextMode = N->blend_mode ? N->blend_mode : (N->rgba[3] != 1.f ? 1 : 0);
			rgba = partrender::LerpLayerColor(rgba, curMode, {N->rgba[0], N->rgba[1], N->rgba[2], N->rgba[3]},
				nextMode, t, s.uniDialect);
			// PAT layer: the next frame's same layer must also be a PAT layer to pair part sets.
			if (L.usePat && N->usePat) {
				layer.nextSpriteId = N->spriteId;
				layer.curWeight = t;
			}
		} else {
			rgba.a = curMode ? L.rgba[3] : 1.f;
		}
		// UNI2/MBTL keep AFRT per layer (Han6_LoadFrameAF, layer +32).
		layer.AFRT = frame.AF.AFRT || L.afrt;
		if (s.mbaacc) {
			layer.cgActorMatrix = cgtransform::Actor(s.angleTurns, s.mirrored, s.pivotX, s.pivotY);
		} else {
			if (s.mirrored) layer.scaleX *= -1.0f;
			layer.rotZ += s.angleTurns;
		}
		layer.blendMode = L.blend_mode;
		// Object priority, then the layer's AFPL draw bucket (UNI2/MBTL;
		// AFPL 0 = object + 256 keeps MBAACC's relative order).
		layer.zPriority = LayerDrawBucket(L.priority, s.zPriority == INT_MIN ? frame.AF.priority : s.zPriority);
		layer.pups = s.pups;
		layer.alpha = rgba.a * s.alphaMul;
		layer.tintColor = glm::vec4(rgba.r * s.tint.r, rgba.g * s.tint.g, rgba.b * s.tint.b, 1.0f);
		layer.isSpawned = s.spawned;
		if (li == 0 && s.boxes) layer.hitboxes = frame.hitboxes;
		if (li == 0 && s.boxes && s.spawned) layer.boxesOutlineOnly = true;
		if (li == 0 && s.patBoxesHalf && L.usePat) layer.boxScale = 0.5f;
		layer.sourceCG = cg;
		layer.usePat = L.usePat;
		layer.sourceParts = parts;
		layer.patApplyPras = partrender::PartUsesPras(s.uniDialect, s.mbtl);
		layer.patUniDialect = s.uniDialect;
		layer.patMbaacc = s.mbaacc;
		layer.patTintCg = s.tintCg;
		layer.spawnFlagset1 = s.flagset1;
		layer.spawnFlagset2 = s.flagset2;
		render.AddLayer(layer);
	}
}

static double MsSince(std::chrono::steady_clock::time_point t0)
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace viewrender

void MainFrame::DrawPresetEffectMarkers(FrameState& state, CharacterInstance* character,
	ImDrawList* drawList, ImVec2 origin, float zoom)
{
	if (!state.vizSettings.showPresetEffects || !state.vizSettings.showSpawnedPatterns) return;
	if (!drawList) return;

	// Preset effects (EF3) and script impact markers are one-tick actors in
	// the preview simulation. "All frames" shows every one the pattern fires.
	FrameData* effectData = character->effectCharacter ? &character->effectCharacter->frameData : nullptr;
	auto& sim = state.BindPreviewSim(&character->frameData, effectData);
	preview::TickState ts;
	sim.getStateAt(state.PreviewTick(&character->frameData), ts);
	const preview::SimActor* root = ts.root();
	const float rootX = root ? root->x : 0.f, rootY = root ? root->y : 0.f;

	struct Marker { float x, y; int preset; };
	std::vector<Marker> markers;
	if (state.vizSettings.presetEffectsAllFrames) {
		sim.ensureSimulatedTo(std::max(state.PreviewTick(&character->frameData), sim.settledTick()));
		for (const auto& r : sim.spawnRecords())
			if (r.isPreset) markers.push_back({r.x - rootX, r.y - rootY, r.pattern});
	} else {
		for (const auto& a : ts.actors)
			if (a.isPreset) markers.push_back({a.x - rootX, a.y - rootY, a.pattern});
	}

	for (const auto& m : markers) {
		const ImVec2 center(origin.x + m.x * zoom, origin.y + m.y * zoom);
		const float size = 15.0f;
		const ImU32 color = IM_COL32(255, 128, 0, 255);  // Orange
		const float thickness = 2.0f;
		drawList->AddLine(ImVec2(center.x - size, center.y), ImVec2(center.x + size, center.y), color, thickness);
		drawList->AddLine(ImVec2(center.x, center.y - size), ImVec2(center.x, center.y + size), color, thickness);
		drawList->AddCircleFilled(center, 3.0f, color);
		if (state.vizSettings.showLabels) {
			const char* presetName = m.preset >= 0 ? GetPresetEffectName(m.preset) : TXT("Impact FX");
			char label[64];
			snprintf(label, sizeof(label), "%s [%d]", presetName, m.preset);
			drawList->AddText(ImVec2(center.x + size + 5, center.y - 8), color, label);
		}
	}
}

// Add the root frame and (optionally) the spawned actors of `ts` as layers.
// rootFrameIndex selects the root's frame (the editor's selected frame for the
// live view, the simulated frame for onion/export samples).
void MainFrame::AddSimulatedActorLayers(CharacterView* view, const preview::TickState& ts,
	int rootFrameIndex, int layer0Sprite, bool includeSpawns, bool boxes,
	float alphaMul, const glm::vec3* sampleTint)
{
	CharacterInstance* active = view->getCharacter();
	auto& state = view->getState();
	Sequence* mainSeq = active->frameData.get_sequence(state.pattern);
	if (!mainSeq || mainSeq->frames.empty()) return;
	rootFrameIndex = std::clamp(rootFrameIndex, 0, (int)mainSeq->frames.size() - 1);
	Frame& mainFrame = mainSeq->frames[rootFrameIndex];

	const bool uniDialect = active->frameData.usesUniFormat();
	const bool mbtl = active->frameData.game() == Ha6Game::MBTL;
	viewrender::ActorStyle rootStyle;
	rootStyle.positionKind=0;
	rootStyle.tracePattern = state.pattern; rootStyle.traceFrame = rootFrameIndex;
	rootStyle.mbaacc = active->frameData.game() == Ha6Game::MBAACC;
	rootStyle.uniDialect = uniDialect;
	rootStyle.mbtl = mbtl;
	rootStyle.seq = mainSeq;
	rootStyle.frameIndex = rootFrameIndex;
	// AFHK progress: only when the simulated root is in the frame being shown (playback, timeline ticks);
	// a frame picked in the editor shows its first tick.
	if (const preview::SimActor* r = ts.root(); r && r->pattern == state.pattern && r->frame == rootFrameIndex) {
		rootStyle.ticksInFrame = r->frameTimer;
		rootStyle.loopCounter = r->loopCounter;
	}
	rootStyle.layer0Sprite = layer0Sprite;
	if(state.authoredInspection) rootStyle.ticksInFrame=state.authoredOffset;
	rootStyle.boxes = boxes;
	rootStyle.alphaMul = alphaMul;
	rootStyle.pups = mainSeq->pups;
	rootStyle.patBoxesHalf = active->frameData.patBoxesHalfScale();
	if (sampleTint) rootStyle.tint = *sampleTint;
	viewrender::AddFrameLayers(render, mainFrame, &active->cg, &active->parts, rootStyle);

	if (!includeSpawns) return;
	const preview::SimActor* simRoot = ts.root();
	const float rootX = simRoot ? simRoot->x : 0.f;
	const float rootY = simRoot ? simRoot->y : 0.f;
	const bool rootFacingLeft = simRoot ? simRoot->facingLeft : false;

	for (const auto& actor : ts.actors) {
		if (actor.isRoot || actor.isPreset) continue;  // presets: DrawPresetEffectMarkers

		// Per-entry visibility / alpha / tint from the spawn tree (right pane)
		const SpawnedPatternInfo* entry = FindSpawnTreeEntry(state.spawnedPatterns,
			actor.srcPattern, actor.srcFrame, actor.srcEffectIndex, actor.effectHa6,
			actor.isScript, actor.pattern);
		if (entry && !entry->visible) continue;
		const float vizAlpha = entry ? entry->alpha : 1.0f;
		const glm::vec4 vizTint = (state.vizSettings.enableTint && entry) ? entry->tintColor : glm::vec4(1.0f);

		FrameData* sourceFrameData = &active->frameData;
		CG* sourceCG = &active->cg;
		Parts* sourceParts = &active->parts;
		if (actor.effectHa6 && active->effectCharacter) {
			sourceFrameData = &active->effectCharacter->frameData;
			sourceCG = &active->effectCharacter->cg;
			sourceParts = &active->effectCharacter->parts;
		}
		Sequence* spawnedSeq = sourceFrameData->get_sequence(actor.pattern);
		if (!spawnedSeq || spawnedSeq->frames.empty()) continue;
		if (actor.frame < 0 || actor.frame >= (int)spawnedSeq->frames.size()) continue;

		viewrender::ActorStyle s;
		if(!actor.srcEffectHa6 && !actor.isScript && actor.srcPattern==state.pattern && actor.srcFrame==state.frame) { s.positionKind=1; s.positionIndex=actor.srcEffectIndex; }
		if(!actor.srcEffectHa6 && !actor.isScript && simRoot && actor.parentId==simRoot->id && actor.srcPattern==state.pattern && actor.srcFrame!=state.frame) { s.positionKind=2; s.positionIndex=actor.id; }
		s.tracePattern = actor.pattern; s.traceFrame = actor.frame;
		s.mbaacc = active->frameData.game() == Ha6Game::MBAACC;
		s.offsetX = actor.x - rootX;
		s.offsetY = actor.y - rootY;
		s.mirrored = actor.facingLeft != rootFacingLeft;
		s.angleTurns = actor.angleTurns();
		s.zPriority = actor.zPriority;          // sticky: AF priority 0 keeps the previous value
		s.alphaMul = vizAlpha * state.vizSettings.spawnedOpacity * alphaMul;
		s.tint = glm::vec3(vizTint.r, vizTint.g, vizTint.b);
		if (sampleTint) s.tint *= *sampleTint;
		// A spawned actor's boxes are not this frame's (e.g. a 600x600 attraction box on an
		// effect read as "Hurtbox 1"): hidden unless asked for, and then outline only.
		s.boxes = boxes && state.vizSettings.showSpawnedBoxes;
		s.spawned = true;
		s.pups = spawnedSeq->pups;
		s.uniDialect = uniDialect;
		s.mbtl = mbtl;
		s.seq = spawnedSeq;
		s.frameIndex = actor.frame;
		s.ticksInFrame = actor.frameTimer;
		s.loopCounter = actor.loopCounter;
		s.flagset1 = actor.flagset1;
		s.flagset2 = actor.flagset2;
		if (actor.effectHa6) s.tintCg = &active->cg;   // effects tint from the owner's palette
		s.patBoxesHalf = sourceFrameData->patBoxesHalfScale();
		viewrender::AddFrameLayers(render, spawnedSeq->frames[actor.frame], sourceCG, sourceParts, s);
	}
}

void MainFrame::AddOpponentLayers(CharacterView* view, const preview::TickState& ts, int tick,float alpha,const glm::vec3* tint,bool sample)
{
	auto& st=view->getState();
	if (!st.opponentEnabled) return;
	auto* owner=view->getCharacter();
	st.opponentStatus="MBAACC only";
	if (owner->frameData.game()!=Ha6Game::MBAACC) return;
	CharacterInstance* target=nullptr;
	for (auto& ch:characters) if (viewrender::SameOpponentPath(ch->getTopHA6Path(),st.opponentTargetPath) || viewrender::SameOpponentPath(ch->getTxtPath(),st.opponentTargetPath)) { target=ch.get(); break; }
	if(!target) { auto it=m_opponentCache.find(st.opponentTargetPath); if(it!=m_opponentCache.end()) target=it->second.get(); }
	st.opponentStatus="Open and choose an MBAACC opponent character";
	if (!target || target->frameData.game()!=Ha6Game::MBAACC) return;
	opponent::Scene scene;
	if (st.opponentFollow || (st.authoredInspection && st.absoluteScrub) || sample) {
		auto& sim=st.BindPreviewSim(&owner->frameData,owner->effectCharacter?&owner->effectCharacter->frameData:nullptr);
		scene=opponent::Resolve(owner->frameData,sim.rootVisitTrack(),tick,ExtendedProfileEnabled());
	} else scene=opponent::InspectAuthored(owner->frameData,st.pattern,st.frame,ExtendedProfileEnabled());
	st.opponentStatus="No supported pose at this position";
	if (!scene.found) return;
	const auto authoredPose=scene.pose;
	int releaseLanding=-1;
	if(scene.pose.release) {
		st.opponentStatus="Release vectors disabled";
		if(!st.opponentReleaseVectors) return;
		SubVector motion{};
		st.opponentStatus="Release vector unavailable: load vector.txt";
		if(!vectors.GetPrimaryMotion(scene.pose.vector,motion)) return;
		const int sourceDirection=ts.root() && ts.root()->facingLeft?-1:1;
		int direction=sourceDirection;
		if(scene.pose.flags&2) direction=-direction;
		const auto flight=opponent::ReleaseFlight(scene.pose.x*sourceDirection,scene.pose.y,scene.elapsed,
			motion.x_vel*direction,motion.y_vel,motion.x_acc*direction,motion.y_acc,st.opponentReleaseGround);
		releaseLanding=flight.landing;
		const auto reaction=opponent::ReleasedReaction(target->frameData,scene.pose.pattern,scene.pose.state,scene.elapsed,flight.landing);
		st.opponentStatus="Release reaction destination missing";
		if(!reaction.valid) return;
		scene.pose.pattern=reaction.pattern; scene.pose.state=reaction.state;
		scene.pose.x=flight.x; scene.pose.y=flight.y;
	}
	// Relationship-relative and keep-position modes need an actual opponent
	// transaction. Do not silently present authored XY as an engine result.
	const auto* sourceSeq=owner->frameData.get_sequence(scene.sourcePattern);
	if(sourceSeq && scene.sourceState>=0 && scene.sourceState<(int)sourceSeq->frames.size()) {
		const auto& e=sourceSeq->frames[scene.sourceState].EF[scene.effect];
		if((e.type==4 && e.parameters[2]!=0) || (e.type==14 && e.parameters[6]!=0) || ((e.type==4 || e.type==14) && (e.parameters[3]&16))) {
			st.opponentStatus="Relationship-relative/keep-position mode not modeled yet"; return;
		}
	}
	auto* seq=target->frameData.get_sequence(scene.pose.pattern);
	st.opponentStatus="Target pattern/state missing (not clamped)";
	if (!seq || scene.pose.state<0 || scene.pose.state>=(int)seq->frames.size()) return;
	auto& frame=seq->frames[scene.pose.state];
	st.opponentStatus="PAT opponent poses not supported in this slice";
	for (const auto& layer:frame.AF.layers) if (layer.usePat) return;
	int ax=0,ay=0;
	if (scene.pose.grabLocation>=1 && scene.pose.grabLocation<=5) {
		auto box=frame.hitboxes.find(17+scene.pose.grabLocation);
		if (box==frame.hitboxes.end()) { st.opponentStatus="Required grab anchor box missing"; return; }
		ax=box->second.xy[0]; ay=box->second.xy[1];
	}
	const bool sourceLeft=ts.root()?ts.root()->facingLeft:false;
	viewrender::ActorStyle style;
	style.mbaacc=true; style.mirrored=scene.facingLeft!=sourceLeft;
	style.offsetX=(scene.pose.release?scene.pose.x:(sourceLeft?-scene.pose.x:scene.pose.x))-(style.mirrored?-ax:ax);
	style.offsetY=scene.pose.y-ay;
	style.angleTurns=scene.pose.rotation/10000.f;
	style.pivotX=ax; style.pivotY=ay;
	style.alphaMul=st.opponentOpacity*alpha; style.tint=tint?*tint:glm::vec3(1.f); style.boxes=false;
	style.tracePattern=scene.pose.pattern; style.traceFrame=scene.pose.state;
	style.pups=seq->pups;
	if(!sample && sourceSeq && scene.effect>=0 && st.opponentOpacity>0.f) {
		const auto& sourceEffect=sourceSeq->frames[scene.sourceState].EF[scene.effect];
		const auto slots=authoredPose.release?opponent::ReleaseEditSlots(sourceEffect,ExtendedProfileEnabled()):opponent::HeldEditSlots(sourceEffect,ExtendedProfileEnabled());
		if(slots.writable()) {
			PositionTarget p; p.kind=PositionTarget::OpponentPose; p.index=scene.effect;
			p.sourcePattern=scene.sourcePattern; p.sourceFrame=scene.sourceState;
			p.xParam=slots.x; p.yParam=slots.y; p.rotationParam=slots.rotation;
			p.rotationSign=style.mirrored?-1.f:1.f;
			const float zoom=view->getZoom();
			p.m[0]=(sourceLeft?-1.f:1.f)*zoom; p.m[1]=p.m[2]=0; p.m[3]=zoom;
			p.screen=ImVec2(std::floor(view->camera().panX+SurfaceFor(view).size.x/2.f)+(sourceLeft?-authoredPose.x:authoredPose.x)*zoom,
				std::floor(view->camera().panY+SurfaceFor(view).size.y/2.f)+authoredPose.y*zoom);
			p.label="Opponent pose -> pattern "+std::to_string(p.sourcePattern)+" state "+std::to_string(p.sourceFrame)+" EF "+std::to_string(p.index);
			if(authoredPose.release) p.label="Release origin -> pattern "+std::to_string(p.sourcePattern)+" state "+std::to_string(p.sourceFrame)+" EF "+std::to_string(p.index)+" (recomputes trajectory; ground may clamp preview)";
			if(p.rotationParam<0) p.label+=" (translation only)";
			p.color=IM_COL32(210,120,255,255);
			SurfaceFor(view).opponentTarget=p; SurfaceFor(view).opponentReady=true;
			SurfaceFor(view).opponentVersion=owner->frameData.dataVersion;
			style.positionKind=3; style.positionIndex=scene.effect;
		}
	}
	viewrender::AddFrameLayers(render,frame,&target->cg,&target->parts,style);
	st.opponentStatus=std::string(st.authoredInspection && st.absoluteScrub?"Absolute timeline: ":scene.authored?"Selected state: ":"Playback: ")+std::to_string(scene.pose.pattern)+" / "+std::to_string(scene.pose.state)+" (held pose; acquisition not simulated)";
	if(scene.pose.release) st.opponentStatus="Release approximation: "+std::to_string(scene.pose.pattern)+" / "+std::to_string(scene.pose.state)+" elapsed="+std::to_string(scene.elapsed)+" y="+std::to_string(scene.pose.y)+" landing="+std::to_string(releaseLanding);
}

// Ticks sampled by the onion skin around `tick`, farthest first, split into
// past (before) and future (after) lists. Keyframe mode steps by root frame
// entries instead of fixed tick spacing.
static void CollectOnionTicks(preview::PreviewSim& sim, const OnionSkinSettings& o, int tick,
	std::vector<int>& past, std::vector<int>& future)
{
	past.clear(); future.clear();
	const int before = std::clamp(o.before, 0, 16), after = std::clamp(o.after, 0, 16);
	const int settled = sim.settledTick();
	const int last = settled >= 0 ? settled : sim.horizon();
	if (!o.keyframesOnly) {
		const int step = std::max(1, o.spacing);
		for (int k = before; k >= 1; --k) if (tick - k * step >= 0) past.push_back(tick - k * step);
		for (int k = after; k >= 1; --k) if (tick + k * step <= last) future.push_back(tick + k * step);
		return;
	}
	// Keyframe mode: the tick each neighbouring root frame visit starts at.
	sim.ensureSimulatedTo(std::min(last, tick + 64 * std::max(1, after)));
	const auto& track = sim.rootFrameTrack();
	if (track.empty()) return;
	auto frameAt = [&](int t) { return t < (int)track.size() ? track[t] : track.back(); };
	int t = std::min(tick, (int)track.size() - 1);
	while (t > 0 && frameAt(t - 1) == frameAt(tick)) --t;   // start of the current visit
	std::vector<int> starts;
	for (int k = 0; k < before && t > 0; ++k) {
		int s = t - 1;
		while (s > 0 && frameAt(s - 1) == frameAt(t - 1)) --s;
		starts.push_back(s);
		t = s;
	}
	past.assign(starts.rbegin(), starts.rend());
	std::vector<int> ahead;
	int u = std::min(tick, (int)track.size() - 1);
	for (int k = 0; k < after; ++k) {
		const int cur = frameAt(u);
		while (u + 1 < (int)track.size() && frameAt(u + 1) == cur) ++u;
		if (u + 1 >= (int)track.size() || u + 1 > last) break;
		++u;
		ahead.push_back(u);
	}
	future.assign(ahead.rbegin(), ahead.rend());
}

void MainFrame::DrawOnionSkin(CharacterView* view, int tick)
{
	const OnionSkinSettings& o = view->onion();
	CharacterInstance* active = view->getCharacter();
	auto& state = view->getState();
	const auto t0 = std::chrono::steady_clock::now();

	FrameData* effectData = active->effectCharacter ? &active->effectCharacter->frameData : nullptr;
	auto& sim = state.BindPreviewSim(&active->frameData, effectData);
	std::vector<int> past, future;
	if(o.enabled) CollectOnionTicks(sim, o, tick, past, future);

	double simMs = 0.0;
	auto drawSample = [&](int t, int distance, const glm::vec3& tint) {
		const auto s0 = std::chrono::steady_clock::now();
		sim.getStateAt(t, m_onionState);
		simMs += viewrender::MsSince(s0);
		const preview::SimActor* root = m_onionState.root();
		if (!root) return;
		float alpha = std::clamp(o.alpha, 0.f, 1.f);
		for (int k = 1; k < distance; ++k) alpha *= std::clamp(o.falloff, 0.f, 1.f);
		render.ClearLayers();
		AddSimulatedActorLayers(view, m_onionState, root->frame, INT_MIN,
			o.includeSpawns && state.vizSettings.showSpawnedPatterns, false, alpha, &tint);
		Sequence* seq = active->frameData.get_sequence(state.pattern);
		const int prio = (seq && root->frame >= 0 && root->frame < (int)seq->frames.size())
			? seq->frames[root->frame].AF.priority : 0;
		render.SortLayersByZPriority(prio);
		render.DrawLayers();
	};
	// Farthest samples first so nearer ones draw on top; all behind the
	// current frame.
	for (size_t i = 0; i < past.size(); ++i) drawSample(past[i], (int)(past.size() - i), o.pastTint);
	for (size_t i = 0; i < future.size(); ++i) drawSample(future[i], (int)(future.size() - i), o.futureTint);
	if(state.opponentEnabled && state.opponentOnion) {
		const auto& oo=view->opponentOnion(); std::vector<int> op,of;
		CollectOnionTicks(sim,oo,tick,op,of);
		auto ghost=[&](int t,int distance,const glm::vec3& tint) {
			sim.getStateAt(t,m_onionState); float alpha=oo.alpha;
			for(int k=1;k<distance;++k) alpha*=oo.falloff;
			render.ClearLayers(); AddOpponentLayers(view,m_onionState,t,alpha,&tint,true);
			render.SortLayersByZPriority(0); render.DrawLayers();
		};
		for(size_t i=0;i<op.size();++i) ghost(op[i],int(op.size()-i),oo.pastTint);
		for(size_t i=0;i<of.size();++i) ghost(of[i],int(of.size()-i),oo.futureTint);
	}

	m_onionStats.samples = (int)(past.size() + future.size());
	m_onionStats.simMs = simMs;
	m_onionStats.totalMs = viewrender::MsSince(t0);
}

void MainFrame::DrawCharacterScene(CharacterView* view, const Render::PassParams& pass, const SceneOptions& opt)
{
	CharacterInstance* active = view ? view->getCharacter() : nullptr;
	if (!active) return;
	render.BeginPass(pass);
	render.cgZoomFilter = gSettings.mbtlCgZoomFilter && active->frameData.game() == Ha6Game::MBTL;
	auto& state = view->getState();

	if (opt.grid) {
		// The PAT editor's origin cross / UV rectangle live in the grid line
		// buffer (RenderUpdate); every other view draws the plain grid.
		if (!view->isPatEditor()) render.ResetGridLines();
		render.DrawGridLines();
	}

	// PAT editor of an RBO / GOF2 character: one pose (or the sprite sheet), never the character's frames or their boxes. The sheet
	// selection (and in the UV view the selected cut-out) is shown as a box in sheet pixels: the sheet is drawn with its top-left at the origin.
	if (view->isPatEditor() && active->frameData.isHan2() && active->parts.loaded) {
		render.ClearLayers();
		RenderLayer layer;
		layer.spriteId = state.renderMode != RenderMode::DEFAULT ? 0 : state.partSet;
		layer.usePat = true; layer.sourceParts = &active->parts; layer.sourceCG = &active->cg;
		if (state.renderMode != RenderMode::DEFAULT) {
			const int sheet = state.renderMode == RenderMode::UV_SETTING_VIEW && active->parts.GetCutOut(state.partCutOut) ? active->parts.GetCutOut(state.partCutOut)->texture : state.partGraph;
			const auto* g = active->parts.GetPartGfx(sheet);
			if (state.sheetSelValid) layer.hitboxes[25] = Hitbox{{state.sheetSel[0], state.sheetSel[1], state.sheetSel[0] + state.sheetSel[2], state.sheetSel[1] + state.sheetSel[3]}};
			if (state.renderMode == RenderMode::UV_SETTING_VIEW && g && g->w > 0)
				if (const auto* co = active->parts.GetCutOut(state.partCutOut)) {
					auto px = [&](int u, int size) { return (int)std::lround(u * size / 256.0); };
					layer.hitboxes[1] = Hitbox{{px(co->uv[0], g->w), px(co->uv[1], g->h), px(co->uv[0] + co->uv[2], g->w), px(co->uv[1] + co->uv[3], g->h)}};
				}
		}
		render.AddLayer(layer);
		render.DrawLayers();
		return;
	}
	Sequence* mainSeq = active->frameData.get_sequence(state.pattern);
	if (!mainSeq || mainSeq->frames.empty()) return;
	// Self-heal a stale frame index (pattern shrunk by undo/reload while the
	// Main Pane's clamp didn't run, e.g. when that pane is hidden).
	if (state.frame < 0) state.frame = 0;
	if (state.frame >= (int)mainSeq->frames.size()) state.frame = (int)mainSeq->frames.size() - 1;

	if(opt.tick<0) SurfaceFor(view).size=ImVec2(float(render.CurrentPass().width),float(render.CurrentPass().height));
	const bool spawns = opt.spawns < 0 ? state.vizSettings.showSpawnedPatterns : opt.spawns != 0;
	const int tick = opt.tick >= 0 ? opt.tick : state.PreviewTick(&active->frameData);

	auto& pick=state.positionSelection;
	if(opt.tick<0 && !m_posDrag.active) pick.sync(state.pattern,state.frame,tick,active->frameData.dataVersion);
	const bool soloLive=opt.tick<0 && view->getBoxPane() && view->getBoxPane()->positionTool && pick.solo.kind>=0;
	if (!soloLive && opt.onion && (view->onion().enabled || (state.opponentEnabled && state.opponentOnion)) && !view->isPatEditor())
		DrawOnionSkin(view, tick);
	else if (view == getActiveView())
		m_onionStats = OnionStats{};

	// Spawned actors at the tick come from the cached tick simulator
	// (preview_sim.h). Positions are world coordinates with the root at the
	// origin; facing is the engine's resolved child facing and the actor angle
	// already includes inherited rotation.
	FrameData* effectData = active->effectCharacter ? &active->effectCharacter->frameData : nullptr;
	preview::TickState& ts = m_sceneState;
	ts.actors.clear();
	ts.tick = -1;
	if (spawns || opt.tick >= 0 || state.opponentEnabled) {
		auto& sim = state.BindPreviewSim(&active->frameData, effectData);
		sim.getStateAt(tick, ts);
	}
	int rootFrame = state.frame;
	// The main window's view keeps the editor's sprite id for layer 0
	// (RenderUpdate refreshes it every frame); other views read the layer.
	int layer0Sprite = view == getActiveView() ? state.spriteId : INT_MIN;
	if (opt.tick >= 0) {
		// Export / explicit tick: the root frame is the simulated one.
		const preview::SimActor* root = ts.root();
		rootFrame = root ? root->frame : 0;
		layer0Sprite = INT_MIN;
	}

	render.ClearLayers();
	AddSimulatedActorLayers(view, ts, rootFrame, layer0Sprite, spawns, opt.boxes, 1.f, nullptr);
	if(opt.tick<0) SurfaceFor(view).opponentReady=false;
	if (opt.tick<0) AddOpponentLayers(view,ts,tick); // no implicit inclusion in exports/onions yet
	// Pattern comparison overlay (issue #63): drawn over the tool windows'
	// target view (the tab worked in last, main or detached) at its live
	// tick, not over onion/export tick samples.
	if (opt.tick < 0 && view == getToolView())
		AddCompareLayers(view, active);
	rootFrame = std::clamp(rootFrame, 0, (int)mainSeq->frames.size() - 1);
	render.SortLayersByZPriority(mainSeq->frames[rootFrame].AF.priority);
	if(soloLive) {
		const auto key=pick.solo; const int index=key.instance>=0?key.instance:key.index;
		if(!render.IsolatePositionTarget(key.kind,index)) pick.solo={}; // missing instance restores the scene
	}
	render.capturePosition=opt.tick<0;
	if(render.capturePosition) render.positionFootprints.clear();
	render.DrawLayers();
	if(render.capturePosition) { SurfaceFor(view).footprints=render.positionFootprints;
		SurfaceFor(view).pattern=state.pattern; SurfaceFor(view).frame=state.frame; SurfaceFor(view).tick=tick; }
	render.capturePosition=false;
	render.FinishTransformTrace();
}

void MainFrame::DrawMainViewScene(CharacterView* view, int width, int height)
{
	Render::PassParams pass;
	pass.width = width;
	pass.height = height;

	if (view->isStageView()) {
		// bgCamera.panLastX/Y is the screen anchor (where world (0, 0) sits)
		// in u4ick's model; the pass origin translates bg sprites, grid and
		// reference markers together when the user pans.
		pass.zoom = view->getZoom();
		pass.originX = bgCamera.panLastX * pass.zoom;
		pass.originY = bgCamera.panLastY * pass.zoom;
		bgCamera.SetGameCamFromView((float)width, (float)height);
		render.BeginPass(pass);
		// Stage back half (band 0, render priority 10) goes under the grid;
		// the front half (weather + band 1) goes over it.
		bgRenderer.Render(bgCamera, width, height, bg::Pass::Back);
		render.ResetGridLines();
		render.DrawGridLines();
		bgRenderer.Render(bgCamera, width, height, bg::Pass::Front);

		// The game's 640x480 view at the current game camera (zoom 1).
		if (m_showGameViewRect) {
			const float z = bgCamera.zoom;
			const ImVec2 vp = ImGui::GetMainViewport()->Pos;
			const float x0 = vp.x + (bgCamera.camX - 320.0f + bgCamera.panLastX) * z;
			const float y0 = vp.y + (bgCamera.camY - 432.0f + bgCamera.panLastY) * z;
			auto* dl = ImGui::GetBackgroundDrawList();
			dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + 640.0f * z, y0 + 480.0f * z), IM_COL32(255, 255, 255, 200), 0.0f, 0, 1.5f);
			dl->AddText(ImVec2(x0 + 4, y0 + 2), IM_COL32(255, 255, 255, 220), TXT("game view"));
		}
		// u4ick's stage boundary rects (MonoForm.cs:431-435). They live at
		// the LIVE camera position (movingPoint, = bgCamera.panX/panY), at
		// parallax=256 implicitly: during a drag they slide with the cursor
		// at 1:1 while parallax layers shift relative to them.
		if (bgRenderer.IsShowingDebugOverlay()) {
			const float z = bgCamera.zoom;
			const ImVec2 vp = ImGui::GetMainViewport()->Pos;   // desktop coords with viewports
			const float px = vp.x + (bgCamera.panX - bg::STAGE_CENTER_X) * z;
			const float py = vp.y + (bgCamera.panY - bg::STAGE_FLOOR_Y) * z;
			auto* dl = ImGui::GetBackgroundDrawList();
			dl->AddLine(ImVec2(px - 401*z, py + 224*z), ImVec2(px + 656*z, py + 224*z),
			            IM_COL32(255, 255, 0, 255), 1.0f);
			dl->AddRect(ImVec2(px - 401*z, py - 538*z), ImVec2(px + 656*z, py + 272*z),
			            IM_COL32(160,  32, 240, 255), 0.0f, 0, 1.0f);
			dl->AddRect(ImVec2(px - 401*z, py + 272*z), ImVec2(px + 656*z, py + 280*z),
			            IM_COL32(160,  32, 240, 255), 0.0f, 0, 1.0f);
		}
		return;
	}

	const ViewCamera& cam = view->camera();
	pass.zoom = cam.zoom;
	pass.originX = cam.panX + width / 2.f;
	pass.originY = cam.panY + height / 2.f;
	render.BeginPass(pass);
	// A loaded stage only renders in its own stage tab; for character tabs
	// the background renderer is disabled and these passes are no-ops.
	bgRenderer.Render(bgCamera, width, height, bg::Pass::Back);
	SceneOptions opt;
	opt.grid = m_showAxis;     // L (PAT tab) / G
	opt.boxes = m_showBoxes;   // H (character tab)
	DrawCharacterScene(view, pass, opt);
	bgRenderer.Render(bgCamera, width, height, bg::Pass::Front);

	const ImVec2 screenOrigin(ImGui::GetMainViewport()->Pos.x + pass.originX, ImGui::GetMainViewport()->Pos.y + pass.originY);
	if (view->getCharacter() && view->getState().positionSelection.solo.kind<0)
		DrawPresetEffectMarkers(view->getState(), view->getCharacter(), ImGui::GetBackgroundDrawList(),
			screenOrigin, pass.zoom);
	if (view->isPatEditor())
		DrawPatSelectionOutline(view, ImGui::GetBackgroundDrawList(), screenOrigin, pass.zoom);
}

void MainFrame::DrawBack()
{
	render.filter = smoothRender;
	ProcessPendingExport();

	// Dev hook: HANTEI_STAGE_PREVIEW=<stage.dat> opens that stage on the
	// first frame and captures the stage view to C:/dev/bg_dump.png ~2 s
	// later (used to eyeball renderer changes without the file dialog).
	static bool stageEnvChecked = false;
	if (!stageEnvChecked) {
		stageEnvChecked = true;
		if (const char* p = std::getenv("HANTEI_STAGE_PREVIEW")) {
			loadStageFile(p);
			bgRenderer.RequestDebugDump(120);
		}
	}

	// Tick background animation (once per frame, regardless of which view
	// is shown).
	bgRenderer.Update();
	// Stage-view smooth zoom-to-cursor: ease the stage tab's zoom toward the
	// target and re-pin the cursor's world anchor each frame so the point
	// under the cursor stays put as the scale changes.
	if (bgZoomAnimating) {
		auto* zv = getActiveView();
		if (!zv || !zv->isStageView()) {
			bgZoomAnimating = false;  // left the stage tab - abandon
		} else {
			float z = zv->getZoom();
			z += (bgZoomTarget - z) * 0.30f;
			const float d = bgZoomTarget - z;
			if (d < 0.004f && d > -0.004f) {
				z = bgZoomTarget;
				bgZoomAnimating = false;
			}
			const float s = z > 0.0f ? z : 1.0f;
			const float zpx = bgZoomAnchorScrnX / s - bgZoomAnchorWorldX;
			const float zpy = bgZoomAnchorScrnY / s - bgZoomAnchorWorldY;
			bgCamera.SetPan(zpx, zpy);
			zoom_idx = z;
			zv->setZoom(z);
			zv->setStageRenderXY(zpx, zpy);
		}
	}
	if (auto* sv = getActiveView(); sv && sv->isStageView())
		bgCamera.zoom = sv->getZoom();
	// Ease panLast -> panX after a drag so the parallax delta decays
	// smoothly instead of snapping (no-op while dragging or settled).
	bgCamera.Settle();

	// --size WxH (headless export / batch `size`) renders the view at that size whatever the window is.
	const int width = std::max(1, gStartup.sizeW > 0 ? gStartup.sizeW : (int)clientRect.x);
	const int height = std::max(1, gStartup.sizeH > 0 ? gStartup.sizeH : (int)clientRect.y);
	const float* bgColor = gStartup.hasBg ? gStartup.bg : clearColor;   // --bg: shot colour, never saved
	CharacterView* view = getActiveView();
	RenderTarget* target = view ? &view->renderTarget() : nullptr;
	if (target && target->ensure(width, height, false)) {
		{
			ScopedTargetBinding bind(*target);
			bind.clear(bgColor[0], bgColor[1], bgColor[2], 1.f, true);
			const auto t0 = std::chrono::steady_clock::now();
			DrawMainViewScene(view, width, height);
			m_lastSceneMs = viewrender::MsSince(t0);
		}
		target->blitToDefault(width, height);
	} else {
		glViewport(0, 0, width, height);
		glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	RenderDetachedViewTargets();
}

#endif /* UI_VIEW_RENDER_IMPL_H_GUARD */
