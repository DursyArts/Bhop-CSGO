#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <thread>
#include <mmsystem.h>
#include <thread>
#include "SlimMem.h"

SlimUtils::SlimMem mem;
DWORD pid;


struct Offset {
	DWORD dwLocalPlayer = 0xCBD6A4;
	DWORD m_fFlags = 0x104;
	DWORD dwForceJump = 0x5170E70;
	int m_iTeamNum = 0xF4;
	int m_iCrosshairId = 0xB394;
	DWORD dwEntityList = 0x4CCDCBC;
	bool m_bDormant = 0xED;
	int dwForceAttack = 0x30FF378;
	int TotalHitCount = 0xA388;
	DWORD m_iHealth = 0x100;
	DWORD m_iGlowIndex = 0xA3F8;
	DWORD dwGlowObjectManager = 0x520DA80;
}O;
struct Values {
	DWORD localPlayer;
	DWORD gameModule;
	BYTE flag;
	int LocalTeam;
	int CrossHairID;
	int EnemyTeam;
	DWORD EnemyInCH;
}V;


bool GetMod()
{
	if (!mem.HasProcessHandle())
		return false;
	if (!mem.ParseModules())
		return false;

	const SlimUtils::SlimModule *mod;
	if ((mod = mem.GetModule(L"client_panorama.dll")) == nullptr)
	{
		mem.ParseModules();
		return false;
	}
	std::cout << mod << std::endl;
	V.gameModule = mod->ptrBase;

	return true;
}

bool GetPlayer()
{
	V.localPlayer = mem.Read<DWORD>(V.gameModule + O.dwLocalPlayer);


	if (V.localPlayer == NULL) {
		V.localPlayer = mem.Read<DWORD>(V.gameModule + O.dwLocalPlayer);
		return false;
	}
	return true;

}

void bhop()
{
	V.flag = mem.Read<BYTE>(V.localPlayer + O.m_fFlags);
	if (GetAsyncKeyState(VK_SPACE) && V.flag & (1 << 0)) // if on ground then jump
	{
		mem.Write<DWORD>(V.gameModule + O.dwForceJump, 6);
	}
}

void trigger()
{
	
}

void hitsound()
{
	int hitcount = mem.Read<int>(V.localPlayer + O.TotalHitCount);
	static int oldhit = hitcount;
	std::cout << oldhit << std::endl;
	std::cout << hitcount << std::endl;
	if (hitcount > oldhit)
	{
		oldhit++;
		PlaySound(("hit.wav"), NULL, SND_ASYNC);
		std::cout << hitcount << std::endl;
	}
	if (oldhit > 1)
	{
		oldhit = hitcount;
	}
}

int main()
{
	mem.GetPID(L"csgo.exe", &pid);
	if (pid == NULL) {
		return false;
	}
	std::cout << "pid: " << pid << std::endl;
	Sleep(500);
	mem.Open(pid, SlimUtils::ProcessAccess::Full);
	GetMod();
	GetPlayer();

	while (true)
	{
		GetPlayer();
		int LocalPlayer_inCross = mem.Read<int>(V.localPlayer + O.m_iCrosshairId);
		int LocalPlayer_Team = mem.Read<int>(V.localPlayer + O.m_iTeamNum);

		DWORD Trigger_EntityBase = mem.Read<DWORD>(V.gameModule + O.dwEntityList + ((LocalPlayer_inCross - 1) * 0x10));
		int Trigger_EntityTeam = mem.Read<int>(Trigger_EntityBase + O.m_iTeamNum);

		if ((LocalPlayer_inCross > 0 && LocalPlayer_inCross <= 64) && (Trigger_EntityBase != NULL) && (Trigger_EntityTeam != LocalPlayer_Team))
		{
			mem.Write<int>(V.gameModule + O.dwForceAttack, 5);
			Sleep(100);
			mem.Write<int>(V.gameModule + O.dwForceAttack, 4);
			Sleep(45);
		}
		bhop();
		hitsound();
	}

	return 0;
}
