#include "pat_limits.h"
#include "../i18n.h"

namespace patlimits {

std::vector<std::string> Check(size_t partSets, size_t cutOuts, const std::vector<size_t>& partsPerSet)
{
	std::vector<std::string> out;
	if (partSets > (size_t)kMaxPartSetId + 1)
		out.push_back(std::to_string(partSets) + TXT(" part sets: frames can only reach part set ids 0..65535."));
	if (cutOuts > (size_t)kMaxCutOuts)
		out.push_back(std::to_string(cutOuts) + TXT(" parts (cut-outs): the game has 2000 slots and writes past them."));
	for (size_t i = 0; i < partsPerSet.size(); ++i)
		if (partsPerSet[i] > (size_t)kMaxPartsPerSet)
			out.push_back(std::string(TXT("Part set ")) + std::to_string(i) + ": " + std::to_string(partsPerSet[i]) +
				TXT(" part slots: the game holds 80 per part set and writes past them."));
	return out;
}

} // namespace patlimits
