#pragma once
#include "IEntity.h"




class CEntity : public IEntity
{
public:
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();	
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	virtual __int64 Export(__int64* a2) = 0;
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	SUBFUNC();
	virtual __int64 ForEachOwnedEntity(__int64 a2) = 0;
	virtual __int64 ForEachDirectlyOwnedEntity(void(__fastcall*** a2)(__int64, unsigned __int64*)) = 0;
	SUBFUNC();
	SUBFUNC();
	virtual __int64 OwnedEntityCount() = 0;
	virtual __int64 OwnedEntityByIndex(unsigned int nIndex) = 0;
	SUBFUNC();
	SUBFUNC();

private:
};



class ISCPlayer
{
public:

private:
};

class CSCPlayer : ISCPlayer
{
public:


private:
};


class CryEntitySystem
{

};


