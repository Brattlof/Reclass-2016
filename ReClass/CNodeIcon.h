#pragma once

#include "CNodeBase.h"

class CNodeIcon : public CNodeBase
{
public:
	virtual NodeSize Draw(ViewInfo& View, int x, int y);
};

