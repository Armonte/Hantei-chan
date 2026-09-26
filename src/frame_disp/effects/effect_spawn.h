#ifndef EFFECT_SPAWN_H_GUARD
#define EFFECT_SPAWN_H_GUARD

// ============================================================================
// Effect Types 1, 101, 11, 111: Pattern Spawning
// ============================================================================
// Type 1:   Spawn Pattern (absolute positioning)
// Type 101: Spawn Relative Pattern (relative to parent)
// Type 1000: Spawn Pattern once (same as 1, guarded by variable p6)
// Type 11:  Spawn Random Pattern (random selection)
// Type 111: Spawn Random Relative Pattern (random + relative)
//
// EF11/111 layout verified in IDA (MBAA Effect11_SpawnRandomPatterns 0x454E30);
// matches the Gonptechan EX fork's layout for p[0]-p[7] and adds p[8]-p[10].
//
// These effects spawn child patterns (projectiles, additional effects, etc.)
// with various positioning, inheritance, and behavior options.
// ============================================================================

static inline void DrawEffectSpawn_Type1_101(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	constexpr float width = 75.f;

	// Check if this is a relative spawn (Type 101)
	bool isRelative = (effect.type == 101);

	// Pattern dropdown with names
			if(frameData) {
				im::SetNextItemWidth(width*3);
				
				// For relative spawns, show as offset notation
				std::string currentName;
				if(isRelative) {
					// Display as "Offset: +N" or "Offset: -N"
					currentName = "Offset: ";
					if(no >= 0) currentName += "+";
					currentName += std::to_string(no);
					
					// Also show the target pattern name if valid
					int targetPattern = patternIndex + no;
					if(targetPattern >= 0 && targetPattern < frameData->get_sequence_count()) {
						currentName += " -> " + frameData->GetDecoratedName(targetPattern);
					}
				} else {
					// Absolute spawn - show pattern name directly
					currentName = frameData->GetDecoratedName(no);
				}
				
				if(im::BeginCombo("Pattern", currentName.c_str())) {
					if(isRelative) {
						// For Type 101: Show relative offsets
						for(int i = 0; i < frameData->get_sequence_count(); i++) {
							int offset = i - patternIndex;
							bool selected = (no == offset);
							
							// Format: "Offset: +5 -> Pattern 105: Name"
							std::string name = "Offset: ";
							if(offset >= 0) name += "+";
							name += std::to_string(offset) + " -> " + frameData->GetDecoratedName(i);
							
							if(im::Selectable(name.c_str(), selected)) {
								no = offset;  // Store the offset
								markModified();
							}
							if(selected)
								im::SetItemDefaultFocus();
						}
					} else {
						// For Type 1: Show absolute patterns
						for(int i = 0; i < frameData->get_sequence_count(); i++) {
							bool selected = (no == i);
							std::string name = frameData->GetDecoratedName(i);
							if(im::Selectable(name.c_str(), selected)) {
								no = i;
								markModified();
							}
							if(selected)
								im::SetItemDefaultFocus();
						}
					}
					im::EndCombo();
				}
			} else {
				im::SetNextItemWidth(width);
				const char* label = isRelative ? "Pattern Offset" : "Pattern";
				if(im::InputInt(label, &no, 0, 0)) {
					markModified();
				}
			}

			// Offset
			im::SetNextItemWidth(width);
			im::DragInt("Offset X", &p[0]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			im::DragInt("Offset Y", &p[1]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			// Flagset 1
			if(im::TreeNode("Flagset 1 (Spawn Behavior)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags1", (unsigned int*)&p[2], &flagIdx, 13)) {
					markModified();
				}

				// Tooltips based on hovered flag
				switch(flagIdx) {
					case 0: Tooltip("Remove when parent gets hit (includes shield, throw, not clash/armor)"); break;
					case 1: Tooltip("Always face right"); break;
					case 2: Tooltip("Follow parent"); break;
					case 3: Tooltip("Affected by hitstop of parent"); break;
					case 4: Tooltip("Coordinates relative to camera"); break;
					case 5: Tooltip("Remove when parent changes pattern"); break;
					case 6: Tooltip("Can go below floor level"); break;
					case 7: Tooltip("Always on floor"); break;
					case 8: Tooltip("Inherit parent's rotation"); break;
					case 9: Tooltip("Position relative to screen border in the back"); break;
					case 10: Tooltip("Unknown effect"); break;
					case 11: Tooltip("Flip facing"); break;
					case 12: Tooltip("Unknown (Akiha's 217 only)"); break;
				}

				im::Text("Raw value: %d", p[2]);
				im::TreePop();
			}

			// Flagset 2
			if(im::TreeNode("Flagset 2 (Child Properties)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags2", (unsigned int*)&p[3], &flagIdx, 13)) {
					markModified();
				}

				// Tooltips based on hovered flag
				switch(flagIdx) {
					case 0: Tooltip("Child has shadow"); break;
					case 1: Tooltip("Advance child with parent (also activates Flagset 1 bit 5)"); break;
					case 2: Tooltip("Remove when parent is thrown"); break;
					case 3: Tooltip("Parent is affected by hitstop"); break;
					case 4: case 5: Tooltip("Unknown effect"); break;
					case 6: Tooltip("Unaffected by parent's Type 2 superflash if spawned during it"); break;
					case 7: Tooltip("Doesn't move with camera"); break;
					case 8: Tooltip("Position is relative to opponent"); break;
					case 9: Tooltip("Position is relative to (-32768, 0)"); break;
					case 10: Tooltip("Top Z Priority (overridden if projectile sets own priority)"); break;
					case 11: Tooltip("Face according to player slot"); break;
					case 12: Tooltip("Unaffected by any superflash"); break;
				}

				im::Text("Raw value: %d", p[3]);
				im::TreePop();
			}

			// Angle
			im::SetNextItemWidth(width);
			im::DragInt("Angle", &p[7]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Clockwise rotation: 0=0°, 2500=90°, 5000=180°, 10000=360°");
			}

			// Projectile var decrease
	im::SetNextItemWidth(width);
	im::DragInt("Proj var decrease", &p[8]);
	if(im::IsItemEdited()) {
		if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
	}
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}

	if(effect.type == 1000) {
		// MBAA Effect1_SpawnPattern: EF1000 spawns only while var word[+0x1C0 + 2*p6] == 0, then sets it to 1.
		im::SetNextItemWidth(width);
		if(im::InputInt("Once-guard variable", &p[5], 0, 0)) markModified();
		im::SameLine(); im::TextDisabled("(?)");
		if(im::IsItemHovered()) Tooltip("Spawns only if this variable is 0, then sets it to 1\n(e.g. Sion's Dust of Osiris)");
	}
}

static inline void DrawEffectSpawn_Type11_111(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	constexpr float width = 75.f;

	// Check if this is a relative spawn (Type 111)
	bool isRelative = (effect.type == 111);

	// Similar to type 1, but for random patterns
			if(frameData) {
				im::SetNextItemWidth(width*3);
				
				// For relative spawns, show as offset notation
				std::string currentName;
				if(isRelative) {
					// Display as "Offset: +N" or "Offset: -N"
					currentName = "Offset: ";
					if(no >= 0) currentName += "+";
					currentName += std::to_string(no);
					
					// Also show the target pattern name if valid
					int targetPattern = patternIndex + no;
					if(targetPattern >= 0 && targetPattern < frameData->get_sequence_count()) {
						currentName += " -> " + frameData->GetDecoratedName(targetPattern);
					}
				} else {
					// Absolute spawn - show pattern name directly
					currentName = frameData->GetDecoratedName(no);
				}
				
				if(im::BeginCombo("Pattern", currentName.c_str())) {
					if(isRelative) {
						// For Type 111: Show relative offsets
						for(int i = 0; i < frameData->get_sequence_count(); i++) {
							int offset = i - patternIndex;
							bool selected = (no == offset);
							
							// Format: "Offset: +5 -> Pattern 105: Name"
							std::string name = "Offset: ";
							if(offset >= 0) name += "+";
							name += std::to_string(offset) + " -> " + frameData->GetDecoratedName(i);
							
							if(im::Selectable(name.c_str(), selected)) {
								no = offset;  // Store the offset
								markModified();
							}
							if(selected)
								im::SetItemDefaultFocus();
						}
					} else {
						// For Type 11: Show absolute patterns
						for(int i = 0; i < frameData->get_sequence_count(); i++) {
							bool selected = (no == i);
							std::string name = frameData->GetDecoratedName(i);
							if(im::Selectable(name.c_str(), selected)) {
								no = i;
								markModified();
							}
							if(selected)
								im::SetItemDefaultFocus();
						}
					}
					im::EndCombo();
				}
			} else {
				im::SetNextItemWidth(width);
				const char* label = isRelative ? "Pattern Offset" : "Pattern";
				if(im::InputInt(label, &no, 0, 0)) {
					markModified();
				}
			}

			// Spawn rectangle
			im::SetNextItemWidth(width);
			if(im::InputInt("Offset X", &p[0], 0, 0)) markModified();
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Offset Y", &p[1], 0, 0)) markModified();
			im::SetNextItemWidth(width);
			if(im::InputInt("Random width", &p[2], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip("X += random(0, width)\nIf Random height = 30000: radius of a random circle instead");
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Random height", &p[3], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip("Y += random(0, height)\n30000 = circular mode (random angle, radius = Random width)");

			im::SetNextItemWidth(width);
			if(im::InputInt("Pattern range", &p[4], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip("Patterns Pattern .. Pattern+range-1 (0 = 1)");
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Spawn count", &p[5], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip("0 = spawn every pattern of the range once, in order;\notherwise spawn this many, each a random pattern from the range");

			// Same flagsets as type 1
			if(im::TreeNode("Flagset 1 (Spawn Behavior)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags1", (unsigned int*)&p[6], &flagIdx, 13)) {
					markModified();
				}

				switch(flagIdx) {
					case 0: Tooltip("Remove when parent gets hit (includes shield, throw, not clash/armor)"); break;
					case 1: Tooltip("Always face right"); break;
					case 2: Tooltip("Follow parent"); break;
					case 3: Tooltip("Affected by hitstop of parent"); break;
					case 4: Tooltip("Coordinates relative to camera"); break;
					case 5: Tooltip("Remove when parent changes pattern"); break;
					case 6: Tooltip("Can go below floor level"); break;
					case 7: Tooltip("Always on floor"); break;
					case 8: Tooltip("Inherit parent's rotation"); break;
					case 9: Tooltip("Position relative to screen border in the back"); break;
					case 10: Tooltip("Unknown effect"); break;
					case 11: Tooltip("Flip facing"); break;
					case 12: Tooltip("Unknown (Akiha's 217 only)"); break;
				}

				im::Text("Raw value: %d", p[6]);
				im::TreePop();
			}

			if(im::TreeNode("Flagset 2 (Child Properties)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags2", (unsigned int*)&p[7], &flagIdx, 13)) {
					markModified();
				}

				switch(flagIdx) {
					case 0: Tooltip("Child has shadow"); break;
					case 1: Tooltip("Advance child with parent (also activates Flagset 1 bit 5)"); break;
					case 2: Tooltip("Remove when parent is thrown"); break;
					case 3: Tooltip("Parent is affected by hitstop"); break;
					case 4: case 5: Tooltip("Unknown effect"); break;
					case 6: Tooltip("Unaffected by parent's Type 2 superflash if spawned during it"); break;
					case 7: Tooltip("Doesn't move with camera"); break;
					case 8: Tooltip("Position is relative to opponent"); break;
					case 9: Tooltip("Position is relative to (-32768, 0)"); break;
					case 10: Tooltip("Top Z Priority (overridden if projectile sets own priority)"); break;
					case 11: Tooltip("Face according to player slot"); break;
					case 12: Tooltip("Unaffected by any superflash"); break;
				}

				im::Text("Raw value: %d", p[7]);
				im::TreePop();
			}

			// Angle
			im::SetNextItemWidth(width);
			im::DragInt("Angle", &p[8]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Clockwise rotation: 0=0°, 2500=90°, 5000=180°, 10000=360°");
			}
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Random angle", &p[9], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip("Angle += random(0, value)");

			// Projectile var decrease
	im::SetNextItemWidth(width);
	im::DragInt("Proj var decrease", &p[10]);
	if(im::IsItemEdited()) {
		if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
	}
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}
}

#endif /* EFFECT_SPAWN_H_GUARD */
