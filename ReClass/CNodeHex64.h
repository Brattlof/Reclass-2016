#pragma once

#include "CNodeBase.h"

class CNodeHex64 : public CNodeBase
{
public:
	CNodeHex64( );

	virtual void Update( HotSpot& Spot );

	virtual ULONG GetMemorySize( ) { return 8; }

	virtual int Draw( ViewInfo& View, int x, int y );
};
