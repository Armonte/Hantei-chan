#ifndef VECTORS_H_GUARD
#define VECTORS_H_GUARD

#include <imgui.h>
#include <cstdio>

struct SubVector
{
	int id;
	int x_vel;
	int y_vel;
	int x_acc;
	int y_acc;
};

struct Vector {
	int id = -1;
	int VecCnt;
	int UkemiTime;
	int Prio;
	int PrioAni;
	int KoCheck;
	int VecNum;
	int HitAni;
	int GuardAni;
	int Time;
	int VecTime;
	int flag;
	int VecNum2 = -1;
	int HitAni2;
	int GuardAni2;
	int Time2;
	int VecTime2;
	int flag2;

	char name[16];
};

class VectorTXT {
protected:
	struct SubVectorList
	{
		SubVector subvectors[256];
	};
	SubVectorList* sub_vector_list;
	struct VectorList
	{
		Vector vectors[256];
	};
	VectorList *vector_list;
public:
	bool loaded;
	bool load(const char* name);

	void free();

	bool drawWindow = false;
	void Draw()
	{
		if (!drawWindow)
			return;

		ImGui::Begin("Vectors Guide", &drawWindow);

		if (!loaded)
		{
			ImGui::Text("No vector.txt loaded.\nTo load data, go to File > Load vector.txt...");
			return;
		}

		if (!ImGui::BeginTable("Vector Guide", 26,
			ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
			ImGuiTableFlags_NoHostExtendX))
			return;

		ImGui::TableSetupColumn("No.");
		ImGui::TableSetupColumn("VecCnt");
		ImGui::TableSetupColumn("UkemiTime");
		ImGui::TableSetupColumn("Prio");
		ImGui::TableSetupColumn("PrioAni");
		ImGui::TableSetupColumn("KoCheck");
		ImGui::TableSetupColumn("VecNum");
		ImGui::TableSetupColumn("HitAni");
		ImGui::TableSetupColumn("GuardAni");
		ImGui::TableSetupColumn("Time");
		ImGui::TableSetupColumn("VecTime");
		ImGui::TableSetupColumn("flag");
		ImGui::TableSetupColumn("X Vel");
		ImGui::TableSetupColumn("Y Vel");
		ImGui::TableSetupColumn("X Acc");
		ImGui::TableSetupColumn("Y Acc");
		ImGui::TableSetupColumn("VecNum");
		ImGui::TableSetupColumn("HitAni");
		ImGui::TableSetupColumn("GuardAni");
		ImGui::TableSetupColumn("Time");
		ImGui::TableSetupColumn("VecTime");
		ImGui::TableSetupColumn("flag");
		ImGui::TableSetupColumn("X Vel");
		ImGui::TableSetupColumn("Y Vel");
		ImGui::TableSetupColumn("X Acc");
		ImGui::TableSetupColumn("Y Acc");
		ImGui::TableHeadersRow();

		int i = 0;
		Vector curVector;
		SubVector* svlp = (SubVector*)sub_vector_list; //sub_vector_list_pointer
		SubVector curSubVector;
		char buffer[64];
		for (int i = 0; i < 256; i++)
		{
			curVector = vector_list->vectors[i];
			if (curVector.id == -1) continue;
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.id);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.VecCnt);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.UkemiTime);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.Prio);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.PrioAni);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.KoCheck);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.VecNum);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.HitAni);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.GuardAni);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.Time);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.VecTime);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%08i", curVector.flag);
			ImGui::Text(buffer);
			curSubVector = svlp[curVector.VecNum];
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curSubVector.x_vel);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curSubVector.y_vel);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curSubVector.x_acc);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curSubVector.y_acc);
			ImGui::Text(buffer);

			if (curVector.VecCnt != 2) continue;
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.VecNum2);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.HitAni2);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.GuardAni2);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.Time2);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curVector.VecTime2);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%08i", curVector.flag2);
			ImGui::Text(buffer);
			curSubVector = svlp[curVector.VecNum2];
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curSubVector.x_vel);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curSubVector.y_vel);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curSubVector.x_acc);
			ImGui::Text(buffer);
			ImGui::TableNextColumn();
			snprintf(buffer, 64, "%i", curSubVector.y_acc);
			ImGui::Text(buffer);
		}

		ImGui::EndTable();

		ImGui::End();
	}

	VectorTXT();
	~VectorTXT();
};

#endif /* VECTORS_H_GUARD */
