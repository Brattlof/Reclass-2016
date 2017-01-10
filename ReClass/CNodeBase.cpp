#include "stdafx.h"

#include "CNodeBase.h"

CNodeBase::CNodeBase( ) :
	m_nodeType( nt_base ),
	m_pParentNode( nullptr ),
	m_Offset( 0 ),
	m_strOffset( _T( "0" ) ),
	m_bHidden( false ),
	m_bSelected( false )
{
	// Optimized
	m_LevelsOpen.resize( 32, false );
	m_LevelsOpen[0] = true;

	// This is the class name
	m_strName.Format( _T( "N%0.8X" ), g_NodeCreateIndex++ );
}

int CNodeBase::FindNode( CNodeBase * pNode )
{ 
	auto found = std::find( Nodes.begin( ), Nodes.end( ), pNode );
	return (found != Nodes.end( )) ? (int)(found - Nodes.begin( )) : -1;
}

// Incorrect view.address
void CNodeBase::AddHotSpot( ViewInfo& View, CRect& Spot, CString Text, int ID, int Type )
{
	if (Spot.top > View.client->bottom || Spot.bottom < 0)
		return;

	HotSpot spot;
	spot.Rect = Spot;
	spot.Text = Text.GetString( );
	spot.Address = View.Address + m_Offset;
	spot.ID = ID;
	spot.Type = Type;
	spot.object = this;
	spot.Level = View.Level;
	View.HotSpots->push_back( spot );
}

int CNodeBase::AddText( ViewInfo& View, int x, int y, DWORD color, int HitID, const wchar_t* fmt, ... )
{
	if (fmt == NULL)
		return x;

	wchar_t wcsbuf[1024] = { 0 };

	va_list va_alist;
	va_start( va_alist, fmt );
	_vsnwprintf( wcsbuf, ARRAYSIZE( wcsbuf ), fmt, va_alist );
	va_end( va_alist );

	int width = (int)(wcslen( wcsbuf )) * g_FontWidth;

	if ((y >= -(g_FontHeight)) && (y + g_FontHeight <= View.client->bottom + g_FontHeight))
	{
		CRect pos;
		if (HitID != HS_NONE)
		{
			if (width >= g_FontWidth * 2)
				pos.SetRect( x, y, x + width, y + g_FontHeight );
			else
				pos.SetRect( x, y, x + g_FontWidth * 2, y + g_FontHeight );

			AddHotSpot( View, pos, wcsbuf, HitID, HS_EDIT );
		}

		pos.SetRect( x, y, 0, 0 );

		View.dc->SetTextColor( color );
		View.dc->SetBkMode( TRANSPARENT );
		View.dc->DrawText( wcsbuf, pos, DT_LEFT | DT_NOCLIP | DT_NOPREFIX );
	}

	return x + width;
}

int CNodeBase::AddText( ViewInfo& View, int x, int y, DWORD color, int HitID, const char* fmt, ... )
{
	char buffer[1024] = { 0 };
	TCHAR finalBuffer[1024] = { 0 };

	if (fmt == NULL)
		return x;

	va_list va_alist;
	va_start( va_alist, fmt );
	_vsnprintf_s( buffer, 1024, fmt, va_alist );
	va_end( va_alist );

	#ifdef UNICODE
	size_t converted = 0; 
	mbstowcs_s( &converted, finalBuffer, buffer, 1024 );
	#else
	memcpy( &finalBuffer, buffer, 1024 );
	#endif

	int width = static_cast<int>(strlen( buffer )) * g_FontWidth;

	if ((y >= -g_FontHeight) && (y + g_FontHeight <= View.client->bottom + g_FontHeight))
	{
		CRect pos;
		if (HitID != HS_NONE)
		{
			if (width >= g_FontWidth * 2)
				pos.SetRect( x, y, x + width, y + g_FontHeight );
			else
				pos.SetRect( x, y, x + g_FontWidth * 2, y + g_FontHeight );

			AddHotSpot( View, pos, finalBuffer, HitID, HS_EDIT );
		}

		pos.SetRect( x, y, 0, 0 );

		View.dc->SetTextColor( color );
		View.dc->SetBkMode( TRANSPARENT );
		View.dc->DrawText( finalBuffer, pos, DT_LEFT | DT_NOCLIP | DT_NOPREFIX );
	}

	return x + width;
}

int CNodeBase::AddAddressOffset( ViewInfo& View, int x, int y )
{
	if (g_bOffset)
	{
		#ifdef _WIN64
		// goto 722
		// just the left side 0000
		// TODO: fix the ghetto rig FontWidth * x
		// where x = characters over 8
		//x += FontWidth; // we need this either way
		//int numdigits = Utils::NumDigits(View.Address);
		//if (numdigits < 8 && numdigits > 4)
		//	x -= ((8 - numdigits) * FontWidth);
		//if (numdigits > 8)
		//	x += ((numdigits - 8) * FontWidth);

		x = AddText( View, x, y, g_crOffset, HS_NONE, _T( "%0.4X" ), m_Offset ) + g_FontWidth;
		#else
		x = AddText( View, x, y, g_crOffset, HS_NONE, _T( "%0.4X" ), m_Offset ) + g_FontWidth;
		#endif
	}

	if (g_bAddress)
	{
		#ifdef _WIN64
		x = AddText( View, x, y, g_crAddress, HS_ADDRESS, _T( "%0.9I64X" ), View.Address + m_Offset ) + g_FontWidth;
		#else
		x = AddText( View, x, y, g_crAddress, HS_ADDRESS, _T( "%0.8X" ), View.Address + m_Offset ) + g_FontWidth;
		#endif
	}

	return x;
}

void CNodeBase::AddSelection( ViewInfo& View, int x, int y, int Height )
{
	if ((y > View.client->bottom) || (y + Height < 0))
		return;

	if (m_bSelected)
		View.dc->FillSolidRect( 0, y, View.client->right, Height, g_crSelect );

	CRect pos( 0, y, 1024, y + Height );
	AddHotSpot( View, pos, CString( ), 0, HS_SELECT );
}

int CNodeBase::AddIcon( ViewInfo& View, int x, int y, int idx, int ID, int Type )
{
	if ((y > View.client->bottom) || (y + 16 < 0))
		return x + 16;

	DrawIconEx( View.dc->m_hDC, x, y, g_Icons[idx], 16, 16, 0, NULL, DI_NORMAL );

	if (ID != -1)
	{
		CRect pos( x, y, x + 16, y + 16 );
		AddHotSpot( View, pos, CString( ), ID, Type );
	}

	return x + 16;
}

int CNodeBase::AddOpenClose( ViewInfo& View, int x, int y )
{
	if ((y > View.client->bottom) || (y + 16 < 0))
		return x + 16;
	return m_LevelsOpen[View.Level] ? AddIcon( View, x, y, ICON_OPEN, 0, HS_OPENCLOSE ) : AddIcon( View, x, y, ICON_CLOSED, 0, HS_OPENCLOSE );
}

void CNodeBase::AddDelete( ViewInfo& View, int x, int y )
{
	if ((y > View.client->bottom) || (y + 16 < 0))
		return;

	if (m_bSelected)
		AddIcon( View, View.client->right - 16, y, ICON_DELETE, 0, HS_DELETE );
}

//void CNodeBase::AddAdd(ViewInfo& View,int x,int y)
//{
//	if ( (y > View.client->bottom) || (y+16 < 0) ) return;
//	if (m_bSelected)AddIcon(View,16,y,ICON_ADD,HS_NONE,HS_NONE);
//}

void CNodeBase::AddTypeDrop( ViewInfo& View, int x, int y )
{
	if (View.bMultiSelected || ((y > View.client->bottom) || (y + 16 < 0)))
		return;

	if (m_bSelected)
		AddIcon( View, 0, y, ICON_DROPARROW, 0, HS_DROP );
}

int CNodeBase::ResolveRTTI( ULONG_PTR Address, int &x, ViewInfo& View, int y )
{
	#ifdef _WIN64
	ULONG_PTR ModuleBase = 0;

	ULONG_PTR RTTIObjectLocatorPtr = 0;
	ULONG_PTR RTTIObjectLocator = 0;

	ULONG ClassHierarchyDescriptorOffset = 0;
	ULONG_PTR ClassHierarchyDescriptor = 0;

	ULONG NumBaseClasses = 0;

	DWORD BaseClassArrayOffset = 0;
	ULONG_PTR BaseClassArray = 0;

	CString RTTIString;

	// Get module base that this address falls in
	ModuleBase = GetModuleBaseFromAddress( Address );
	if (!ModuleBase)
		return x;

	RTTIObjectLocatorPtr = Address - sizeof( ULONG64 ); // Address is Ptr to first VFunc, pRTTI is at -0x8
	if (!IsValidPtr( RTTIObjectLocatorPtr ))
		return x;

	ReClassReadMemory( (LPVOID)RTTIObjectLocatorPtr, &RTTIObjectLocator, sizeof( ULONG_PTR ) );

	ReClassReadMemory( (LPVOID)(RTTIObjectLocator + 0x10), &ClassHierarchyDescriptorOffset, sizeof( ULONG ) );

	ClassHierarchyDescriptor = ModuleBase + ClassHierarchyDescriptorOffset;
	if (!IsValidPtr( ClassHierarchyDescriptor ) || !ClassHierarchyDescriptorOffset)
		return x;

	ReClassReadMemory( (LPVOID)(ClassHierarchyDescriptor + 0x8), &NumBaseClasses, sizeof( ULONG ) );
	if (NumBaseClasses < 0 || NumBaseClasses > 25)
		NumBaseClasses = 0;

	ReClassReadMemory( (LPVOID)(ClassHierarchyDescriptor + 0xC), &BaseClassArrayOffset, sizeof( ULONG ) );

	BaseClassArray = ModuleBase + BaseClassArrayOffset;
	if (!IsValidPtr( BaseClassArray ) || !BaseClassArrayOffset)
		return x;

	for (ULONG i = 0; i < NumBaseClasses; i++)
	{
		ULONG BaseClassDescriptorOffset = 0;
		ULONG_PTR BaseClassDescriptor = 0;

		ULONG TypeDescriptorOffset = 0;
		ULONG_PTR TypeDescriptor = 0;

		if (i != 0 && i != NumBaseClasses)
		{
			RTTIString += TEXT( " : " ); // Base class
		}

		ReClassReadMemory( (LPVOID)(BaseClassArray + (sizeof( ULONG ) * i)), &BaseClassDescriptorOffset, sizeof( ULONG ) );

		BaseClassDescriptor = ModuleBase + BaseClassDescriptorOffset;
		if (!IsValidPtr( BaseClassDescriptor ) || !BaseClassDescriptorOffset)
			continue;

		ReClassReadMemory( (LPVOID)BaseClassDescriptor, &TypeDescriptorOffset, sizeof( ULONG ) );

		TypeDescriptor = ModuleBase + TypeDescriptorOffset;
		if (!IsValidPtr( TypeDescriptor ) || !TypeDescriptorOffset)
			continue;

		CString RTTIName;
		BOOLEAN FoundEnd = FALSE;
		CHAR LastChar = ' ';
		for (int j = 1; j < 45; j++)
		{
			CHAR RTTINameChar;
			ReClassReadMemory( (LPVOID)(TypeDescriptor + 0x10 + j), &RTTINameChar, 1 );
			if (RTTINameChar == '@' && LastChar == '@') // Names are ended with @@
			{
				FoundEnd = TRUE;
				RTTIName += RTTINameChar;
				break;
			}
			RTTIName += RTTINameChar;
			LastChar = RTTINameChar;
		}

		// Did we find a valid RTTI name or did we just reach end of loop
		if (FoundEnd == TRUE)
		{
			TCHAR Demangled[MAX_PATH] = { 0 };
			if (_UnDecorateSymbolName( RTTIName, Demangled, MAX_PATH, UNDNAME_NAME_ONLY ) == 0)
			{
				RTTIString += RTTIName;
			}
			else
			{
				RTTIString += Demangled;
			}
		}
	}
	#else	
	ULONG_PTR RTTIObjectLocatorPtr = 0;
	ULONG_PTR RTTIObjectLocator = 0;

	ULONG_PTR ClassHierarchyDescriptorPtr = 0;
	ULONG_PTR ClassHierarchyDescriptor = 0;

	ULONG NumBaseClasses = 0;

	ULONG_PTR BaseClassArrayPtr = 0;
	ULONG_PTR BaseClassArray = 0;

	CString RTTIString;

	RTTIObjectLocatorPtr = Address - sizeof( ULONG );
	if (!IsValidPtr( RTTIObjectLocatorPtr ))
		return x;

	ReClassReadMemory( (LPVOID)RTTIObjectLocatorPtr, &RTTIObjectLocator, sizeof( ULONG_PTR ) );

	ClassHierarchyDescriptorPtr = RTTIObjectLocator + 0x10;
	if (!IsValidPtr( ClassHierarchyDescriptorPtr ))
		return x;

	ReClassReadMemory( (LPVOID)ClassHierarchyDescriptorPtr, &ClassHierarchyDescriptor, sizeof( ULONG_PTR ) );

	ReClassReadMemory( (LPVOID)(ClassHierarchyDescriptor + 0x8), &NumBaseClasses, sizeof( ULONG ) );
	if (NumBaseClasses < 0 || NumBaseClasses > 25)
		NumBaseClasses = 0;

	BaseClassArrayPtr = ClassHierarchyDescriptor + 0xC;
	if (!IsValidPtr( BaseClassArrayPtr ))
		return x;

	ReClassReadMemory( (LPVOID)BaseClassArrayPtr, &BaseClassArray, sizeof( ULONG_PTR ) );
	
	for (ULONG i = 0; i < NumBaseClasses; i++)
	{
		ULONG_PTR BaseClassDescriptorPtr = 0;
		ULONG_PTR BaseClassDescriptor = 0;

		ULONG_PTR TypeDescriptor = 0; // Ptr at 0x00 in BaseClassDescriptor

		if (i != 0 && i != NumBaseClasses)
		{
			RTTIString += " : "; // Base class
		}

		BaseClassDescriptorPtr = BaseClassArray + (4 * i);
		if (!IsValidPtr( BaseClassDescriptorPtr ))
			continue;

		ReClassReadMemory( (LPVOID)BaseClassDescriptorPtr, &BaseClassDescriptor, sizeof( ULONG_PTR ) );
		if (!IsValidPtr( BaseClassDescriptor ))
			continue;

		ReClassReadMemory( (LPVOID)BaseClassDescriptor, &TypeDescriptor, sizeof( ULONG_PTR ) );

		CString RTTIName;
		BOOLEAN FoundEnd = FALSE;
		CHAR LastChar = ' ';

		for (int j = 1; j < 45; j++)
		{
			CHAR RTTINameChar;
			ReClassReadMemory( (LPVOID)(TypeDescriptor + 0x08 + j), &RTTINameChar, 1 );
			if (RTTINameChar == '@' && LastChar == '@') // Names seem to be ended with @@
			{
				FoundEnd = TRUE;
				RTTIName += RTTINameChar;
				break;
			}

			RTTIName += RTTINameChar;
			LastChar = RTTINameChar;
		}

		// Did we find a valid RTTI name or did we just reach end of loop
		if (FoundEnd == TRUE)
		{
			TCHAR Demangled[256] = { 0 };

			if (_UnDecorateSymbolName( RTTIName.GetString( ), Demangled, 256, UNDNAME_NAME_ONLY ) == 0)
			{
				RTTIString += RTTIName.GetString( );
			}
			else
			{
				RTTIString += Demangled;
			}
		}
	}
	#endif

	x = AddText( View, x, y, g_crOffset, HS_RTTI, _T( "%s" ), RTTIString.GetString( ) );

	return x;
}

int CNodeBase::AddComment( ViewInfo& View, int x, int y )
{
	x = AddText( View, x, y, g_crComment, HS_NONE, _T( "//" ) );
	// Need the extra whitespace in "%s " after the %s to edit.
	x = AddText( View, x, y, g_crComment, HS_COMMENT, _T( "%s " ), m_strComment );

	if (m_nodeType == nt_hex64)
	{
		float flVal = *(float*)&View.pData[m_Offset];
		LONG_PTR intVal = *(LONG_PTR*)&View.pData[m_Offset];

		if (g_bFloat)
		{
			if (flVal > -99999.0 && flVal < 99999.0)
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%0.3f)" ), flVal );
			else
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%0.3f)" ), 0.0f );
		}

		if (g_bInt)
		{
			if (intVal > 0x6FFFFFFF && intVal < 0x7FFFFFFFFFFF)
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%I64d|0x%IX)" ), intVal, intVal );
			else if (intVal == 0)
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%I64d)" ), intVal );
			else
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%I64d|0x%X)" ), intVal, intVal );
		}

		// *** this is probably broken, let's fix it after
		ULONG_PTR uintVal = (ULONG_PTR)intVal;
		CString strAddress( GetAddressName( uintVal, FALSE ) );
		if (strAddress.GetLength( ) > 0)
		{
			if (g_bPointers)
			{
				//printf( "<%p> here\n", Val );
				if (uintVal > 0x6FFFFFFF && uintVal < 0x7FFFFFFFFFFF)
				{
					x = AddText( View, x, y, g_crOffset, HS_EDIT, _T( "*->%s " ), strAddress.GetString( ) );
					if (g_bRTTI)
						x = ResolveRTTI( uintVal, x, View, y );

					// Print out info from PDB at address
					if (g_bSymbolResolution)
					{
						ULONG_PTR ModuleAddress = 0;
						SymbolReader* pSymbols = NULL;

						ModuleAddress = GetModuleBaseFromAddress( uintVal );
						if (ModuleAddress != 0)
						{
							pSymbols = g_ReClassApp.m_pSymbolLoader->GetSymbolsForModuleAddress( ModuleAddress );
							if (!pSymbols)
							{
								CString moduleName = GetModuleName( uintVal );
								if (!moduleName.IsEmpty( ))
									pSymbols = g_ReClassApp.m_pSymbolLoader->GetSymbolsForModuleName( moduleName );
							}

							if (pSymbols != NULL)
							{
								CString SymbolOut;
								SymbolOut.Preallocate( 1024 );
								if (pSymbols->GetSymbolStringFromVA( uintVal, SymbolOut ))
								{
									x = AddText( View, x, y, g_crOffset, HS_EDIT, _T( "%s " ), SymbolOut.GetString( ) );
								}
							}
						}
					}
				}
			}

			if (g_bString)
			{
				bool bAddStr = true;
				char txt[64];
				ReClassReadMemory( (LPVOID)uintVal, txt, 64 );

				for (int i = 0; i < 8; i++)
				{
					if (!isprint( (unsigned char)txt[i] ))
						bAddStr = false;
				}

				if (bAddStr)
				{
					txt[63] = '\0';
					x = AddText( View, x, y, g_crChar, HS_NONE, _T( "'%hs'" ), txt );
				}
			}
		}
	}
	else if (m_nodeType == nt_hex32)
	{
		float flVal = *((float*)&View.pData[m_Offset]);
		int intVal = *((int*)&View.pData[m_Offset]);

		if (g_bFloat)
		{
			if (flVal > -99999.0 && flVal < 99999.0)
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%0.3f)" ), flVal );
			else
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%0.3f)" ), 0.0f );
		}

		if (g_bInt)
		{
			#ifdef _WIN64
			if (intVal > 0x140000000 && intVal < 0x7FFFFFFFFFFF) // in 64 bit address range
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%i|0x%IX)" ), intVal, intVal );
			else if (intVal > 0x400000 && intVal < 0x140000000)
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%i|0x%X)" ), intVal, intVal );
			else if (intVal == 0)
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%i)" ), intVal );
			else
				x = AddText( View, x, y, g_crValue, HS_NONE, _T( "(%i|0x%X)" ), intVal, intVal );
			#else
			x = (intVal == 0) ? AddText( View, x, y, g_crValue, HS_NONE, _T( "(%i)" ), intVal ) : AddText( View, x, y, g_crValue, HS_NONE, _T( "(%i|0x%X)" ), intVal, intVal );
			#endif
		}

		// *** this is probably broken, let's fix it after
		size_t uintVal = (size_t)intVal;
		CString strAddress( GetAddressName( uintVal, FALSE ) );
		if (strAddress.GetLength( ) > 0)
		{
			if (g_bPointers)
			{
				//#ifdef _WIN64
				// If set max to 0x140000000 a bunch of invalid pointers come up
				// Set to 0x110000000 instead
				if (uintVal > 0x400000 && uintVal < 0x110000000)
				{
					x = AddText( View, x, y, g_crOffset, HS_EDIT, _T( "*->%s " ), strAddress.GetString( ) );
					if (g_bRTTI)
						x = ResolveRTTI( uintVal, x, View, y );

					// Print out info from PDB at address
					if (g_bSymbolResolution)
					{
						ULONG_PTR ModuleAddress = 0;
						SymbolReader* pSymbols = NULL;

						ModuleAddress = GetModuleBaseFromAddress( uintVal );
						if (ModuleAddress != 0)
						{
							pSymbols = g_ReClassApp.m_pSymbolLoader->GetSymbolsForModuleAddress( ModuleAddress );
							if (!pSymbols)
							{
								CString ModuleName = GetModuleName( uintVal );
								if (!ModuleName.IsEmpty( ))
									pSymbols = g_ReClassApp.m_pSymbolLoader->GetSymbolsForModuleName( ModuleName );
							}

							if (pSymbols != NULL)
							{
								CString SymbolOut;
								SymbolOut.Preallocate( 1024 );
								if (pSymbols->GetSymbolStringFromVA( uintVal, SymbolOut ))
								{
									x = AddText( View, x, y, g_crOffset, HS_EDIT, _T( "%s " ), SymbolOut.GetString( ) );
								}
							}
						}
					}
				}
			}

			if (g_bString)
			{
				bool bAddStr = true;
				char txt[64];
				ReClassReadMemory( (LPVOID)uintVal, txt, 64 ); // TODO: find out why it looks wrong

				for (int i = 0; i < 4; i++)
				{
					if (!isprint( (unsigned char)txt[i] ))
						bAddStr = false;
				}

				if (bAddStr)
				{
					txt[63] = '\0'; // null terminte (even though we prolly dont have to)
					x = AddText( View, x, y, g_crChar, HS_NONE, _T( "'%hs'" ), txt );
				}
			}
		}
	}

	return x;
}

void CNodeBase::StandardUpdate( HotSpot &Spot )
{
	if (Spot.ID == HS_NAME)
		m_strName = Spot.Text;
	else if (Spot.ID == HS_COMMENT)
		m_strComment = Spot.Text;
}

int CNodeBase::DrawHidden( ViewInfo& View, int x, int y )
{
	View.dc->FillSolidRect( 0, y, View.client->right, 1, (m_bSelected) ? g_crSelect : g_crHidden );
	return y + 1;
}