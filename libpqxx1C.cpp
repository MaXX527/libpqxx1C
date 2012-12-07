// libpqxx1C.cpp: определяет экспортированные функции для приложения DLL.
//


#include "stdafx.h"


#ifdef __linux__
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#endif

#include <stdio.h>
#include <wchar.h>
#include "libpqxx1C.h"
#include <string>

#define TIME_LEN 34

#define BASE_ERRNO     7

static wchar_t *g_PropNames[] = {L"ColumnsCount", L"RowsCount", L"EOD", L"ErrorDesc"};
static wchar_t *g_MethodNames[] = {L"Connect", L"Disconnect", L"ExecuteSelect", L"NextRow", L"NextCell", L"GetColumnType", L"GetColumnName"};

static wchar_t *g_PropNamesRu[] = {L""};
static wchar_t *g_MethodNamesRu[] = {L""};

static const wchar_t g_kClassNames[] = L"CAddInlibpqxx1C"; //"|OtherClass1|OtherClass2";
static IAddInDefBase *pAsyncEvent = NULL;

uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len = 0);
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len = 0);
uint32_t getLenShortWcharStr(const WCHAR_T* Source);

//===========================================================================//
using std::string;
using std::ofstream;
static pqxx::connection *connection = nullptr;
static pqxx::work *work = nullptr;
static pqxx::result result;
static pqxx::result::const_iterator iterator;
static unsigned short columnCounter = 0;
static BOOL bEOD = true;

void RealString(const char*, int, string&);
void Raise1CException(const char*);
void date_part(const string&, tm*);

#define MAX_STR_SIZE 1024
char pszErrBuff[MAX_STR_SIZE];
//===========================================================================//

//---------------------------------------------------------------------------//
long GetClassObject(const WCHAR_T* wsName, IComponentBase** pInterface)
{
    if(!*pInterface)
    {
        *pInterface= new CAddInlibpqxx1C;
        return (long)*pInterface;
    }
    return 0;
}
//---------------------------------------------------------------------------//
long DestroyObject(IComponentBase** pIntf)
{
   if(!*pIntf)
      return -1;

   delete *pIntf;
   *pIntf = 0;
   return 0;
}
//---------------------------------------------------------------------------//
const WCHAR_T* GetClassNames()
{
    static WCHAR_T* names = 0;
    if (!names)
        ::convToShortWchar(&names, g_kClassNames);
    return names;
}
//---------------------------------------------------------------------------//
#ifndef __linux__
VOID CALLBACK MyTimerProc(
        HWND hwnd, // handle of window for timer messages
        UINT uMsg, // WM_TIMER message
        UINT idEvent, // timer identifier
        DWORD dwTime // current system time
);
#else
static void MyTimerProc(int sig);
#endif //__linux__

// CAddInNative
//---------------------------------------------------------------------------//
CAddInlibpqxx1C::CAddInlibpqxx1C()
{
    m_iMemory = 0;
    m_iConnect = 0;
}
//---------------------------------------------------------------------------//
CAddInlibpqxx1C::~CAddInlibpqxx1C()
{
}
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::Init(void* pConnection)
{ 
    m_iConnect = (IAddInDefBase*)pConnection;
    return m_iConnect != NULL;
}
//---------------------------------------------------------------------------//
long CAddInlibpqxx1C::GetInfo()
{ 
    // Component should put supported component technology version 
    // This component supports 2.0 version
    return 2000; 
}
//---------------------------------------------------------------------------//
void CAddInlibpqxx1C::Done()
{
}
/////////////////////////////////////////////////////////////////////////////
// ILanguageExtenderBase
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::RegisterExtensionAs(WCHAR_T** wsExtensionName)
{ 
    wchar_t *wsExtension = L"libpqxx1C";
    int iActualSize = ::wcslen(wsExtension) + 1;
    WCHAR_T* dest = 0;

    if (m_iMemory)
    {
        if(m_iMemory->AllocMemory((void**)wsExtensionName, iActualSize * sizeof(WCHAR_T)))
            ::convToShortWchar(wsExtensionName, wsExtension, iActualSize);
        return true;
    }

    return false; 
}
//---------------------------------------------------------------------------//
long CAddInlibpqxx1C::GetNProps()
{ 
    // You may delete next lines and add your own implementation code here
    return ePropLast;
}
//---------------------------------------------------------------------------//
long CAddInlibpqxx1C::FindProp(const WCHAR_T* wsPropName)
{ 
    long plPropNum = -1;
    wchar_t* propName = 0;

    ::convFromShortWchar(&propName, wsPropName);
    plPropNum = findName(g_PropNames, propName, ePropLast);

    if (plPropNum == -1)
        plPropNum = findName(g_PropNamesRu, propName, ePropLast);

    delete[] propName;

    return plPropNum;
}
//---------------------------------------------------------------------------//
const WCHAR_T* CAddInlibpqxx1C::GetPropName(long lPropNum, long lPropAlias)
{ 
    if (lPropNum >= ePropLast)
        return NULL;

    wchar_t *wsCurrentName = NULL;
    WCHAR_T *wsPropName = NULL;
    int iActualSize = 0;

    switch(lPropAlias)
    {
    case 0: // First language
        wsCurrentName = g_PropNames[lPropNum];
        break;
    case 1: // Second language
        wsCurrentName = g_PropNamesRu[lPropNum];
        break;
    default:
        return 0;
    }
    
    iActualSize = wcslen(wsCurrentName)+1;

    if (m_iMemory && wsCurrentName)
    {
        if (m_iMemory->AllocMemory((void**)&wsPropName, iActualSize * sizeof(WCHAR_T)))
            ::convToShortWchar(&wsPropName, wsCurrentName, iActualSize);
    }

    return wsPropName;
}
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
	size_t len = 0;
	time_t rawtime;
	tm *t;

    switch(lPropNum)
    {
	case ePropColumnsCount:
		TV_VT(pvarPropVal) = VTYPE_I4;
		TV_I4(pvarPropVal) = result.columns();
		break;
	case ePropRowsCount:
		TV_VT(pvarPropVal) = VTYPE_I4;
		TV_I4(pvarPropVal) = result.size();
		break;
	case ePropEOD:
		TV_VT(pvarPropVal) = VTYPE_BOOL;
        TV_BOOL(pvarPropVal) = bEOD;
		break;
	case ePropErrorDesc:
		len = strnlen(pszErrBuff, MAX_STR_SIZE);
		m_iMemory->AllocMemory((void**)&pvarPropVal->pstrVal, len*sizeof(wchar_t));
		strncpy(pvarPropVal->pstrVal, pszErrBuff, len);
		pvarPropVal->strLen = len;
		TV_VT(pvarPropVal) = VTYPE_PSTR;
		break;
    default:
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::SetPropVal(const long lPropNum, tVariant *varPropVal)
{ 
    switch(lPropNum)
    {
    default:
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::IsPropReadable(const long lPropNum)
{ 
    switch(lPropNum)
    {
	case ePropColumnsCount:
	case ePropRowsCount:
	case ePropEOD:
	case ePropErrorDesc:
        return true;
    default:
        return false;
    }

    return false;
}
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::IsPropWritable(const long lPropNum)
{
    switch(lPropNum)
    {
    default:
        return false;
    }

    return false;
}
//---------------------------------------------------------------------------//
long CAddInlibpqxx1C::GetNMethods()
{ 
    return eMethLast;
}
//---------------------------------------------------------------------------//
long CAddInlibpqxx1C::FindMethod(const WCHAR_T* wsMethodName)
{ 
    long plMethodNum = -1;
    wchar_t* name = 0;

    ::convFromShortWchar(&name, wsMethodName);

    plMethodNum = findName(g_MethodNames, name, eMethLast);

    if (plMethodNum == -1)
        plMethodNum = findName(g_MethodNamesRu, name, eMethLast);

    return plMethodNum;
}
//---------------------------------------------------------------------------//
const WCHAR_T* CAddInlibpqxx1C::GetMethodName(const long lMethodNum, const long lMethodAlias)
{ 
    if (lMethodNum >= eMethLast)
        return NULL;

    wchar_t *wsCurrentName = NULL;
    WCHAR_T *wsMethodName = NULL;
    int iActualSize = 0;

    switch(lMethodAlias)
    {
    case 0: // First language
        wsCurrentName = g_MethodNames[lMethodNum];
        break;
    case 1: // Second language
        wsCurrentName = g_MethodNamesRu[lMethodNum];
        break;
    default: 
        return 0;
    }

    iActualSize = wcslen(wsCurrentName)+1;

    if (m_iMemory && wsCurrentName)
    {
        if(m_iMemory->AllocMemory((void**)&wsMethodName, iActualSize * sizeof(WCHAR_T)))
            ::convToShortWchar(&wsMethodName, wsCurrentName, iActualSize);
    }

    return wsMethodName;
}
//---------------------------------------------------------------------------//
long CAddInlibpqxx1C::GetNParams(const long lMethodNum)
{ 
    switch(lMethodNum)
    {
	case eMethConnect:
		return 1;
	case eMethExecuteSelect:
		return 1;
	case eMethGetColumnType:
		return 1;
	case eMethGetColumnName:
		return 1;
    default:
        return 0;
    }
    
    return 0;
}
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::GetParamDefValue(const long lMethodNum, const long lParamNum,
                          tVariant *pvarParamDefValue)
{ 
    TV_VT(pvarParamDefValue)= VTYPE_EMPTY;

    switch(lMethodNum)
    {
	case eMethConnect:
	case eMethDisconnect:
	case eMethExecuteSelect:
	case eMethNextRow:
	case eMethNextCell:
	case eMethGetColumnType:
	case eMethGetColumnName:
        // There are no parameter values by default 
        break;
    default:
        return false;
    }

    return false;
} 
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::HasRetVal(const long lMethodNum)
{ 
    switch(lMethodNum)
    {
	case eMethConnect:
		return true;
	case eMethNextCell:
		return true;
	case eMethGetColumnType:
		return true;
	case eMethGetColumnName:
		return true;
    default:
        return false;
    }

    return false;
}
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::CallAsProc(const long lMethodNum,
                    tVariant* paParams, const long lSizeArray)
{
	char *pszBuff = {0};
	//ofstream outfile;

    switch(lMethodNum)
    {
//===========================================================================//
	case eMethDisconnect:
		if(!result.empty()) result.clear();
		if(work)
		{
			delete work;
			work = nullptr;
		}
		if(connection)
		{
			connection->disconnect();
			delete connection;
			connection = nullptr;
		}
		break;
	case eMethExecuteSelect:
		work = new pqxx::work(*connection);
		m_iMemory->AllocMemory((void**)(&pszBuff), paParams->wstrLen + 1);
		CharToOemBuff(paParams->pwstrVal, pszBuff, paParams->wstrLen + 1);
		result = work->exec(pszBuff);
		iterator = result.begin();
		if(!result.empty())
			bEOD = false;
		columnCounter = 0;
		break;
	case eMethNextRow:
		if(++iterator == result.end())
			bEOD = true;
		columnCounter = 0;
		break;
//===========================================================================//
    default:
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::CallAsFunc(const long lMethodNum,
                tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{ 
    bool ret = false;
    FILE *file = 0;
    char *name = 0;
    int size = 0;
    char *mbstr = 0;
    wchar_t* wsTmp = 0;

	string sTmp = "";
	char *pszBuff = {0};
	int ColNum = 0;
	unsigned long len = 0L;
	ofstream outfile;
	time_t rawtime;
	tm *t;

    switch(lMethodNum)
    {
//===========================================================================//
	case eMethConnect:
		m_iMemory->AllocMemory((void**)&pszBuff, paParams->wstrLen + 1);
		CharToOemBuff(paParams->pwstrVal, pszBuff, paParams->wstrLen + 1);
		try
		{
			connection = new pqxx::connection(pszBuff);
			connection->set_client_encoding("WIN");
			pvarRetValue->bVal = connection->is_open();
			TV_VT(pvarRetValue) = VTYPE_BOOL;
		}
		catch(const pqxx::sql_error &e)
		{
			Raise1CException(e.what());
		}
		catch(const std::exception &e)
		{
			Raise1CException(e.what());
		}
		catch(...)
		{
			Raise1CException("Unknown error");
		}
		m_iMemory->FreeMemory((void**)&pszBuff);
		if(!connection)
		{
			pvarRetValue->bVal = false;
			TV_VT(pvarRetValue) = VTYPE_BOOL;
		}

		ret = true;
		break;
	case eMethNextCell:
		switch(iterator[columnCounter].type())
		{
		case 1114: // _timestamp
		case 1184: // _timestamptz
			rawtime = time(NULL);
			t = localtime(&rawtime);
			date_part(iterator[columnCounter].as<string>(), t);
			pvarRetValue->tmVal = *t;
			TV_VT(pvarRetValue) = VTYPE_TM;
			break;
		case 20: // _int8
		case 21: // _int2
		case 23: // _int4
			pvarRetValue->intVal = iterator[columnCounter].as<int>();
			TV_VT(pvarRetValue) = VTYPE_I4;
			break;
		case 700: // _float4
		case 701: // _float8
			pvarRetValue->dblVal = iterator[columnCounter].as<double>();
			TV_VT(pvarRetValue) = VTYPE_R8;
			break;
		default: // all other
			sTmp = iterator[columnCounter].as<string>();
			m_iMemory->AllocMemory((void**)&pvarRetValue->pstrVal, sTmp.length() + 1);
			strcpy_s(pvarRetValue->pstrVal, sTmp.length() + 1, sTmp.c_str());
			pvarRetValue->strLen = sTmp.length() + 1;
			TV_VT(pvarRetValue) = VTYPE_PSTR;
			sTmp.empty();
			break;
		}
		columnCounter++;
		ret = true;
		break;
	case eMethGetColumnType:
		ColNum = paParams->intVal;
		pvarRetValue->intVal = result.column_type(ColNum);
		TV_VT(pvarRetValue) = VTYPE_I4;
		ret = true;
		break;
	case eMethGetColumnName:
		ColNum = paParams->intVal;
		len = strnlen(result.column_name(ColNum), MAX_STR_SIZE);
		m_iMemory->AllocMemory((void**)&pvarRetValue->pstrVal, len);
		strncpy(pvarRetValue->pstrVal, result.column_name(ColNum), len);
		pvarRetValue->strLen = len;
		TV_VT(pvarRetValue) = VTYPE_PSTR;
		ret = true;
		break;
//===========================================================================//
    }
    return ret; 
}
//---------------------------------------------------------------------------//
// This code will work only on the client!
/*#ifndef __linux__
VOID CALLBACK MyTimerProc(
  HWND hwnd,    // handle of window for timer messages
  UINT uMsg,    // WM_TIMER message
  UINT idEvent, // timer identifier
  DWORD dwTime  // current system time
)
{
    if (!pAsyncEvent)
        return;

    wchar_t *who = L"ComponentNative", *what = L"Timer";

    wchar_t *wstime = new wchar_t[TIME_LEN];
    if (wstime)
    {
        wmemset(wstime, 0, TIME_LEN);
        ::_ultow(dwTime, wstime, 10);
        pAsyncEvent->ExternalEvent(who, what, wstime);
        delete[] wstime;
    }
}
#else
void MyTimerProc(int sig)
{
    if (!pAsyncEvent)
        return;

    WCHAR_T *who = 0, *what = 0, *wdata = 0;
    wchar_t *data = 0;
    time_t dwTime = time(NULL);

    data = new wchar_t[TIME_LEN];
    
    if (data)
    {
        wmemset(data, 0, TIME_LEN);
        swprintf(data, TIME_LEN, L"%ul", dwTime);
        ::convToShortWchar(&who, L"ComponentNative");
        ::convToShortWchar(&what, L"Timer");
        ::convToShortWchar(&wdata, data);

        pAsyncEvent->ExternalEvent(who, what, wdata);

        delete[] who;
        delete[] what;
        delete[] wdata;
        delete[] data;
    }
}
#endif*/
//---------------------------------------------------------------------------//
void CAddInlibpqxx1C::SetLocale(const WCHAR_T* loc)
{
#ifndef __linux__
    _wsetlocale(LC_ALL, loc);
#else
    //We convert in char* char_locale
    //also we establish locale
    //setlocale(LC_ALL, char_locale);
#endif
}
/////////////////////////////////////////////////////////////////////////////
// LocaleBase
//---------------------------------------------------------------------------//
bool CAddInlibpqxx1C::setMemManager(void* mem)
{
    m_iMemory = (IMemoryManager*)mem;
    return m_iMemory != 0;
}
//---------------------------------------------------------------------------//
void CAddInlibpqxx1C::addError(uint32_t wcode, const wchar_t* source, 
                        const wchar_t* descriptor, long code)
{
    if (m_iConnect)
    {
        WCHAR_T *err = 0;
        WCHAR_T *descr = 0;
        
        ::convToShortWchar(&err, source);
        ::convToShortWchar(&descr, descriptor);

        m_iConnect->AddError(wcode, err, descr, code);
        delete[] err;
        delete[] descr;
    }
}
//---------------------------------------------------------------------------//
long CAddInlibpqxx1C::findName(wchar_t* names[], const wchar_t* name, 
                         const uint32_t size) const
{
    long ret = -1;
    for (uint32_t i = 0; i < size; i++)
    {
        if (!wcscmp(names[i], name))
        {
            ret = i;
            break;
        }
    }
    return ret;
}
//---------------------------------------------------------------------------//
uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len)
{
    if (!len)
        len = ::wcslen(Source)+1;

    if (!*Dest)
        *Dest = new WCHAR_T[len];

    WCHAR_T* tmpShort = *Dest;
    wchar_t* tmpWChar = (wchar_t*) Source;
    uint32_t res = 0;

    ::memset(*Dest, 0, len*sizeof(WCHAR_T));
    do
    {
        *tmpShort++ = (WCHAR_T)*tmpWChar++;
        ++res;
    }
    while (len-- && *tmpWChar);

    return res;
}
//---------------------------------------------------------------------------//
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len)
{
    if (!len)
        len = getLenShortWcharStr(Source)+1;

    if (!*Dest)
        *Dest = new wchar_t[len];

    wchar_t* tmpWChar = *Dest;
    WCHAR_T* tmpShort = (WCHAR_T*)Source;
    uint32_t res = 0;

    ::memset(*Dest, 0, len*sizeof(wchar_t));
    do
    {
        *tmpWChar++ = (wchar_t)*tmpShort++;
        ++res;
    }
    while (len-- && *tmpShort);

    return res;
}
//---------------------------------------------------------------------------//
uint32_t getLenShortWcharStr(const WCHAR_T* Source)
{
    uint32_t res = 0;
    WCHAR_T *tmpShort = (WCHAR_T*)Source;

    while (*tmpShort++)
        ++res;

    return res;
}
//---------------------------------------------------------------------------//

// Велосипедизм во всей красе
void RealString(const char* pStr, int l, string& ret)
{
	char* buf = new char[l];
	unsigned char s = '\0';
	unsigned char prevS = '\0';
	unsigned short w = 0;
	unsigned short j = 0;

	for(int i = 0; i < l; i++)
	{
		prevS = s;
		s = (pStr[i] << 16) >> 16;
		
		if(s >= 128 && w == 0)
		{
			w = 1;
		}
		else if(s < 128 && w == 0)
		{
			buf[j++] = s;
		}
		else
		{
			w = 1;
			if((s + 48 + 64) < 256)
				s += 64;
			buf[j++] = s + 48;
			// Буква ё, мать её
			if(prevS == 209 && s == 145)
				buf[j - 1] -= 9;
			w = 0;
		}
	}
	buf[j] = '\0';
	ret = buf;
}

void Raise1CException(const char* err)
{
	string ret = "";
	RealString(err, strnlen(err, MAX_STR_SIZE), ret);
	strncpy(pszErrBuff, ret.c_str(), ret.length() + 1);
	ret.clear();
	//pAsyncEvent->AddError(ADDIN_E_IMPORTANT, L"AddInlibpqxx1C", L"ERROR", 3001);
}

// "2012-11-29 09:16:05+06" => tm
// Еще один велосипед %:@ный
void date_part(const string& input, tm* t)
{
	string year = input.substr(0, 4);
	string mon = input.substr(5, 2);
	string mday = input.substr(8, 2);
	string hour = input.substr(11, 2);
	string min = input.substr(14, 2);
	string sec = input.substr(17, 2);

	int iYear = atoi(year.c_str()) - 1900;
	int iMon = atoi(mon.c_str()) - 1;
	int iMday = atoi(mday.c_str());
	int iHour = atoi(hour.c_str());
	int iMin = atoi(min.c_str());
	int iSec = atoi(sec.c_str());

	t->tm_year = iYear;
	t->tm_mon = iMon;
	t->tm_mday = iMday;
	t->tm_hour = iHour;
	t->tm_min = iMin;
	t->tm_sec = iSec;
}
