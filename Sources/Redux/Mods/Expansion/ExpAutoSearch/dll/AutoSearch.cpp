// ============================================================================
//  AutoSearch.dll  —  ВОССТАНОВЛЕННЫЙ ИСХОДНИК (reverse-engineered)
//  Мод ExpAutoSearch (авторы Expansion). PE32 i386, MSVC C++ (RTTI/STL), 343 KB.
//  Восстановлено по Hex-Rays из Mods\Expansion\ExpAutoSearch\DATA\AutoSearch.dll
//  (imagebase 0x10000000). Импортирует ТОЛЬКО kernel32 — в движок НЕ линкуется,
//  а ходит по структурам галактики по сырым оффсетам, получив указатель из скрипта.
//
//  НАЗНАЧЕНИЕ: нативный «автопоиск» — перебор ВСЕХ магазинов/корпусов галактики
//  и отбор предметов под заданный фильтр. Это и есть образец «перебор в DLL»:
//  один нативный проход вместо тысяч итераций байткода RScript VM.
//
//  === FFI-привязка (декларативная, в CFG\Main.txt мода) ===
//    MV ^{ ML ^{ ... } ImportedFunctions ^{        // (блок FFI)
//      AutoSearch ^{
//        AutoSearchLaunch          = int,  AutoSearchLaunch,          dword,dword
//        FillAutoSearchBonusArray  = void, FillAutoSearchBonusArray,  int,str,str,str,str,int,str,str,int,str,str,str,str,str,str,str,str,int,int,int,int,int,int,int,int,int,str,int
//        FillAutoSearchDamageSetArray = void, FillAutoSearchDamageSetArray, int,int
//        FillAutoSearchRequestList = void, FillAutoSearchRequestList, str,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,str,int,int,int,int,int
//        GetAutoSearchResult       = dword,GetAutoSearchResult,       int,int
//        InitAutoSearchDLL         = void, InitAutoSearchDLL,         int,dword
//        IsAutoSearchDLLInit       = int,  IsAutoSearchDLLInit
//        Path = Mods\Expansion\ExpAutoSearch\DATA\AutoSearch.dll
//      }
//    }}
//  Формат строки: ИмяВСкрипте = ВозврТип, ИмяЭкспорта, ТипАрг1, ТипАрг2, ...
//  Типы: int / dword / str (widestring) / void. Движок грузит DLL по Path ЛЕНИВО
//  и регистрирует экспорты как штатные SF_-функции с этой сигнатурой. Ни инъекции,
//  ни лаунчера, ни хуков не требуется.  (ABI экспортов: __cdecl.)
//
//  === Вызов из скрипта (ExpAutoSearch.rson) ===
//    FillAutoSearchRequestList(тип_запроса, ...24 параметра фильтра...);
//    cnt = AutoSearchLaunch(GalaxyPtr(), ShipStar(Player()));   // запустить перебор
//    for (j = 0; j < cnt; j = j + 1) {
//        cur_item = GetAutoSearchResult(j, 0);   // найденный предмет
//        cur_shop = GetAutoSearchResult(j, 1);   // магазин, где он лежит
//    }
//
//  ЛЕГЕНДА достоверности:
//    [V]  — проверено по декомпиляции (оффсет/логика точны)
//    [R]  — реконструкция (смысл ясен из контекста, имя поля — предположение)
//    [?]  — назначение поля движка требует сверки с игровым IDB (Rangers*.i64)
// ============================================================================

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

using  dword = uint32_t;
using  wstr  = std::wstring;         // MSVC std::wstring, 24 байта (SSO16+len+cap) [V]

// ----------------------------------------------------------------------------
//  СТРУКТУРЫ ДВИЖКА, как их видит DLL (оффсеты — из AutoSearchLaunch/sub_10003500)
//  Имена полей помечены [?] — точное соответствие сверяется по игровому IDB.
//  Здесь показаны ТОЛЬКО поля, которые реально читает DLL.
// ----------------------------------------------------------------------------
// Delphi TList: { void** items@+4; int count@+8; }  [V] (везде используется [+4]/[+8])
struct TList { void* _pad0; void** items; int count; };

struct Galaxy {                         // a3/a4 в AutoSearchLaunch (GalaxyPtr())
    char  _pad[0x2C];
    TList constellations;               // +0x2C: список созвездий (Count@+8)        [V]
    char  _pad2[0x94 - 0x2C - sizeof(TList)];
    void* conArray;                     // +0x94: массив пар {?, Constellation*} по 8Б [V]
};                                       //        берётся [8*i + 4] = созвездие

struct Constellation {                  // элемент галактики
    char  _pad[0x24];
    TList stars;                        // +0x24: звёзды созвездия                    [R]
    char  _pad2[0x44 - 0x24 - sizeof(TList)];
    wchar_t** name;                     // +0x44: имя (widestring*)                   [?]
    char  _pad3[0x2C];                  // +0x2C относительно... см. ниже
    // ПРИМЕЧАНИЕ: в коде используются ДВА списка созвездия:
    //   +0x24 (stars)  — цикл по «звёздам», каждый star имеет список магазинов в +0x110;
    //   +0x2C (objs)   — цикл по объектам типа 6..13, с под-списком предметов в +0x4D0.
    // Точные роли (планеты/корабли/руины) сверяются с IDB.
};

struct Star {                           // элемент Constellation.stars (v12)
    char _pad[0x74];
    uint8_t kind;                       // +0x74: тип объекта; 5 и 6 пропускаются     [?]
    char _pad2[0x110 - 0x75];
    TList shopItems;                    // +0x110: предметы в магазине (Count@+8)      [R]
};

struct StarObj {                        // элемент Constellation.objs (v17)
    char _pad[0x10];
    uint8_t kind;                       // +0x10: тип; обрабатывается при (kind-6)<=7  [?]
    char _pad2[0x4D0 - 0x11];
    TList shopItems;                    // +0x4D0: предметы (Count@+8)                 [R]
    // У объекта есть метод, возвращающий строку-признак; если == L"SSB" — пропуск.  [V]
    //   (в декомпиляции замаскирован как __std_stacktrace_to_string — это НЕ он,
    //    а виртуальный геттер строки объекта; сверить по IDB.)
};

struct Item {                           // a2 в матчере — предмет в магазине
    char _pad0[12];
    uint8_t  itemType;   // +0x0C  тип предмета (Hull/Weapon/Engine/...)              [V]
    char _pad1[24-13];
    int      v24;        // +0x18  (для type==42: мин-порог; иначе макс)               [V] dword_10052AF8
    char _pad2[28-28];
    int      v28;        // +0x1C  владелец/раса (owner byte)                          [V] через +28
    char _pad2b[32-29];
    int      cost;       // +0x20  стоимость (cf. dword_10052AFC)                      [V]
    char _pad3[60-36];
    wchar_t* bonusName;  // +0x3C  имя бонуса/спец-модификации (widestring)            [V]
    char _pad4[84-64];
    int      subraceIdx; // +0x54  индекс строки BonusArray + 1 (0 => нет)            [V] *(a2+84)
    char _pad5[88-88];
    void*    starsList;  // +0x58  список «родных» систем/серий                        [R]
    char _pad6[100-92];
    int      stat0;      // +0x64  статы предмета (урон/радиус/…) — смещения 100/104/108[V]
    int      stat1;      // +0x68
    int      stat2;      // +0x6C
    char _pad7[128-112];
    void*    eqStats;    // +0x80  блок характеристик снаряжения (+49: спец-флаги)     [V]
};

// ----------------------------------------------------------------------------
//  ГЛОБАЛЬНОЕ СОСТОЯНИЕ  (адреса — из декомпиляции, секция .data 0x10052xxx/54xxx)
// ----------------------------------------------------------------------------
// --- буфер результатов: std::vector<std::pair<Item*,Shop*>> ---  [V]
static dword* g_resFirst = nullptr;     // 0x10054A64 "Src"        (_Myfirst)
static dword* g_resLast  = nullptr;     // 0x10054A68               (_Mylast)
static dword* g_resEnd   = nullptr;     // 0x10054A6C               (_Myend)

// --- таблица бонусов по subrace: массив записей по 392 байта (98 dword) ---  [V]
//     индекс = subraceIdx-1; заполняется FillAutoSearchBonusArray.
struct BonusRec {                       // 392 байта = 0x188                         [V]
    wstr  name;          // +0x000 (a2)
    wstr  w24;           // +0x018 (a3)
    wstr  w48;           // +0x030 (a4)
    int   shipClassMask; // +0x048 (из строки a5: Ranger1 Warrior2 Pirate4 Transport8
                         //          Liner16 Diplomat32 Flagman2048)                 [V]
    int   d76;           // +0x04C (a6)
    wstr  w80;           // +0x050 (a7)
    wstr  w104;          // +0x068 (a8)
    int   d128;          // +0x080 (a9)
    wstr  serie[7];      // +0x084..+0x13B (a10..a16) — 7 имён-серий бонусов         [V]
    // wstr w300 (a17) идёт следом; далее 9 int'ов дельт:
    wstr  w300;          // +0x12C (a17)
    int   d[9];          // +0x144..+0x164 (a18..a26) — пер-стат дельты бонуса        [V]
    wstr  w360;          // +0x168 (a27) — если содержит "NoDelta" => noDelta=1       [V]
    int   noDelta;       // +0x180
    int   d388;          // +0x184 (a28) — флаг «исключить subrace из поиска»         [V] (98*idx+97)
};
static BonusRec* g_bonus = nullptr;     // 0x10054A5C  (выделяется в InitAutoSearchDLL)
static int       g_bonusCount = 0;      //              (хранится в [g_bonus-4])

// --- damage-set / по-типовые флаги бонус-категорий: 18 dword, индекс = itemType-50 --- [V]
static dword*    g_block = nullptr;     // 0x10054A60  (alloc 72 байта)
static dword     g_galaxyArg = 0;       // 0x10054A48  (2-й аргумент InitAutoSearchDLL)
static bool      g_init = false;        // 0x10054A58

// --- ФИЛЬТР текущего запроса (заполняется FillAutoSearchRequestList) ---  [V]
static int  f_itemType   = 0;           // 0x10052A70  (42 Hull,43 Fuel,44 Eng,45 Radar,
                                         //              46 Scan,47 Droid,48 Grip,49 Shield,
                                         //              либо подтип оружия)
static int  f_ownerOk[8] = {0};         // 0x10052A74[0..7] — разрешённые владельцы (==1)
static int  f_shipClass[11]={0};        // 0x10052A94[..]    — для корпусов: класс корабля
static int  f_hullA[7]   = {0};         // 0x10052A98..0x10052ABC — доп. фильтры корпусов
static int  f_bonus0=0,f_bonus1=0,f_bonus2=0; // 0x10052AC0/AC4/AC8 — три-state фильтр бонуса
static wstr f_damageSet;                 // 0x10052ACC — набор типов урона (стр. a19)
static int  f_statMin0=0,f_statMin1=0,f_statMin2=0; // 0x10052AE4/AE8/AEC — мин. пороги статов
static int  f_subrace = 0;               // 0x10052AF0  (>=0 точный; -1/-2 спец-режимы)
static int  f_series  = 0;               // 0x10052AF4  (серия доминаторов)
static int  f_costMax = 0;               // 0x10052AF8
static int  f_costMax2= 0;               // 0x10052AFC
static int  f_domOnly = 0;               // 0x10052B00  (1 => только доминаторские системы)
static int  f_typeLock= -1;              // 0x10052A70-сопутствующий dword_10052A70 режим

// вспомогательные из STL рантайма MSVC (восстанавливать не нужно — это CRT):
//   sub_10001B60 = std::wstring::assign ; sub_100051B0 = assign-в-место ;
//   sub_1001D4A6 = operator delete ; sub_10004910 = wstring::find ;
//   sub_10005B10 = vector::_Emplace_reallocate.

// ----------------------------------------------------------------------------
//  ЭКСПОРТЫ
// ----------------------------------------------------------------------------

// InitAutoSearchDLL(count, galaxyPtr) — выделить таблицу bonus на `count` записей.  [V]
extern "C" __declspec(dllexport)
void InitAutoSearchDLL(int count, dword galaxyArg)
{
    // layout: [int count][ BonusRec x count ]  — заголовок хранит count.
    g_bonusCount = count;
    g_bonus      = new BonusRec[count];     // eh vector ctor на 0x188-байтных записях [V]
    g_block      = (dword*)::operator new(72);
    g_galaxyArg  = galaxyArg;               // 0x10054A48
    g_init       = true;                    // 0x10054A58 = 1
}

// IsAutoSearchDLLInit() -> проинициализирована ли таблица.  [V]
extern "C" __declspec(dllexport)
int IsAutoSearchDLLInit() { return g_init ? 1 : 0; }

// FillAutoSearchDamageSetArray(itemType, flags) — Block[itemType-50] = flags.  [V]
extern "C" __declspec(dllexport)
void FillAutoSearchDamageSetArray(int itemType, int flags)
{
    g_block[itemType - 50] = (dword)flags;
}

// FillAutoSearchBonusArray(idx, name, w24, w48, classStr, d76, w80, w104, d128,
//                          serie0..serie6(a10..a16), w300(a17),
//                          d18..d26, w360(a27), d388(a28))
// Заполняет запись g_bonus[idx] (конфиг доминаторской модификации по subrace).  [V]
extern "C" __declspec(dllexport)
void FillAutoSearchBonusArray(int idx,
    const wchar_t* name, const wchar_t* w24, const wchar_t* w48,
    const wchar_t* classStr, int d76,
    const wchar_t* w80, const wchar_t* w104, int d128,
    const wchar_t* s0,const wchar_t* s1,const wchar_t* s2,const wchar_t* s3,
    const wchar_t* s4,const wchar_t* s5,const wchar_t* s6, const wchar_t* w300,
    int d18,int d19,int d20,int d21,int d22,int d23,int d24,int d25,int d26,
    const wchar_t* w360, int d388)
{
    BonusRec& r = g_bonus[idx];
    if (name) r.name = name;
    if (w24)  r.w24  = w24;
    if (w48)  r.w48  = w48;
    if (classStr) {                         // строка вида "Ranger Pirate ..." -> маска [V]
        wstr c = classStr; int m = 0;
        if (c.find(L"Ranger")    != wstr::npos) m += 1;
        if (c.find(L"Warrior")   != wstr::npos) m += 2;
        if (c.find(L"Pirate")    != wstr::npos) m += 4;
        if (c.find(L"Transport") != wstr::npos) m += 8;
        if (c.find(L"Liner")     != wstr::npos) m += 16;
        if (c.find(L"Diplomat")  != wstr::npos) m += 32;
        if (c.find(L"Flagman")   != wstr::npos) m += 2048;
        r.shipClassMask = m;
    }
    r.d76 = d76;
    if (w80)  r.w80  = w80;
    if (w104) r.w104 = w104;
    r.d128 = d128;
    const wchar_t* s[7] = {s0,s1,s2,s3,s4,s5,s6};
    for (int i = 0; i < 7; ++i) if (s[i]) r.serie[i] = s[i];
    if (w300) r.w300 = w300;
    int dd[9] = {d18,d19,d20,d21,d22,d23,d24,d25,d26};
    for (int i = 0; i < 9; ++i) r.d[i] = dd[i];
    if (w360) { r.w360 = w360; if (r.w360.find(L"NoDelta") != wstr::npos) r.noDelta = 1; }
    r.d388 = d388;
}

// FillAutoSearchRequestList(typeStr, ...24 фильтра...) — задать активный запрос.  [V]
//   typeStr выбирает ветку и смысл аргументов:
//     "Weapons"  -> f_itemType=a18 (подтип оружия); bonus0..2=a10..a12; statMin=a13..a15;
//                   damageSet=a19; owner=a2..a9
//     "Hulls"    -> f_itemType=42;  shipClass/доп.=a10..a16
//     "Engines"  -> f_itemType=44;  statMin0=a10; statMin1=a11
//     "FuelTanks"-> 43;  "Radars"->45;  "Scanners"->46;  "Droids"->47;
//     "Grippers" -> 48;  "ShieldGenerators"->49;  (для них statMin0=a10)
//   Общее для всех: owner=a2..a9; subrace=a20; series=a21; costMax=a22/a23; domOnly=a24.
extern "C" __declspec(dllexport)
void FillAutoSearchRequestList(const wchar_t* typeStr,
    int a2,int a3,int a4,int a5,int a6,int a7,int a8,int a9,          // владельцы[0..7]
    int a10,int a11,int a12,int a13,int a14,int a15,int a16,int a17,  // per-type
    int a18, const wchar_t* a19,                                      // weaponSubtype, damageSet
    int a20,int a21,int a22,int a23,int a24)                          // subrace,series,cost,cost2,domOnly
{
    f_domOnly = a24;
    wstr t = typeStr ? typeStr : L"";
    auto setOwners = [&]{ f_ownerOk[0]=a2;f_ownerOk[1]=a3;f_ownerOk[2]=a4;f_ownerOk[3]=a5;
                          f_ownerOk[4]=a6;f_ownerOk[5]=a7;f_ownerOk[6]=a8;f_ownerOk[7]=a9; };
    auto setTail  = [&]{ f_subrace=a20; f_series=a21; f_costMax=a22; f_costMax2=a23; };

    if (t == L"Weapons") {
        f_itemType = a18;
        if (a19) f_damageSet = a19;
        setOwners();
        f_statMin0=a13; f_statMin1=a14; f_statMin2=a15;
        f_bonus0=a10; f_bonus1=a11; f_bonus2=a12;
        setTail();
    } else if (t == L"Hulls") {
        setOwners();
        f_shipClass[0]=a10; f_hullA[0]=a11; f_hullA[1]=a12; f_hullA[2]=a13;
        f_hullA[3]=a14; f_hullA[4]=a15; f_hullA[5]=a16;
        f_itemType = 42; f_statMin0 = a17; setTail();
    } else if (t == L"Engines") {
        setOwners(); f_statMin0=a10; f_itemType=44; f_statMin1=a11; setTail();
    } else if (t == L"FuelTanks") { f_itemType=43; setOwners(); f_statMin0=a10; setTail(); }
      else if (t == L"Radars")    { f_itemType=45; setOwners(); f_statMin0=a10; setTail(); }
      else if (t == L"Scanners")  { f_itemType=46; setOwners(); f_statMin0=a10; setTail(); }
      else if (t == L"Droids")    { f_itemType=47; setOwners(); f_statMin0=a10; setTail(); }
      else if (t == L"Grippers")  { f_itemType=48; setOwners(); f_statMin0=a10; setTail(); }
      else if (t == L"ShieldGenerators") { f_itemType=49; setOwners(); f_statMin0=a10; setTail(); }
    // f_costMax2 (0x10052AFC) = a23 во всех ветках. [V]
}

// --- внутренняя: проверить один предмет и, если подходит, записать в результаты --- [V]
//     (оригинал sub_10003500; вызывается для каждого предмета каждого магазина)
static void MatchAndCollect(Item* it, void* shop)
{
    if (!g_bonus) throw std::runtime_error("BonusArray was not initialised!"); // [V]

    int type    = it->itemType;             // +0x0C
    int subIdx  = it->subraceIdx - 1;       // +0x54 - 1
    // --- корректировка типа по записи бонуса (d76) для подтипов >=50 ---      [V]
    if (subIdx >= 0 && g_bonus[subIdx].d76) { /* ... сверка type с d76 ... */ }

    // --- фильтр bonus-категории (если тип не зафиксирован) ---                [V]
    if (f_typeLock == -1) {
        if ((unsigned)(type - 50) <= 0x12) {
            int flags = (type >= 68) ? (*((int*)it->eqStats + 49)) : g_block[type - 50];
            int pass = (flags & 1) ? f_bonus0 : (flags & 2) ? f_bonus1
                     : (flags & 4) ? f_bonus2 : f_bonus0;
            if (!pass) return;
        }
    } else if (type != f_typeLock && type != -1) return;

    // --- цена / стоимость ---                                                 [V]
    if (f_costMax && ((type == 42) ? (it->v24 < f_costMax) : (it->v24 > f_costMax))) return;
    if (f_costMax2 && it->cost > f_costMax2) return;
    // --- владелец ---                                                         [V]
    if (f_ownerOk[(uint8_t)it->v28] != 1) return;
    // --- имя бонуса != "SubFaction" и т.п. строковые сверки ---               [V]
    // (сравнение it->bonusName с эталоном; при несовпадении — return)
    // --- subrace / series ---                                                 [V]
    //   f_subrace >= 0: точное совпадение; -1: «любой заселённый»; -2: спец.
    // --- пороги статов по типу (switch на type: 42/44/'+'/'-'/'.'/'/'/'0'/...) --- [V]
    //   сравнивает it->stat0/stat1/stat2 (+ дельта g_bonus[subIdx].d[k]) с f_statMin*.

    // ВСЕ фильтры пройдены -> push_back({item, shop})                          [V]
    dword pair[2] = { (dword)it, (dword)shop };
    if (g_resLast == g_resEnd) { /* sub_10005B10: рост вектора x1.5 */ }
    *g_resLast++ = pair[0];  *g_resLast++ = pair[1];   // (схематично)
}

// AutoSearchLaunch(galaxy, playerStar) — ГЛАВНЫЙ ПЕРЕБОР. Возврат = число найденного. [V]
extern "C" __declspec(dllexport)
int AutoSearchLaunch(dword galaxyPtr, dword playerStar)
{
    g_resLast = g_resFirst;                               // очистить результаты    [V]
    Galaxy* gx = (Galaxy*)galaxyPtr;

    for (int c = 0; c < gx->constellations.count; ++c)    // по созвездиям          [V]
    {
        Constellation* con = (Constellation*)((void**)gx->conArray)[2*c + 1]; // [8*c+4]

        // созвездия «особого» типа (faction==20) пропускаются при domOnly         [V]
        // if (f_domOnly && con->faction == 20) continue;

        // (1) звёзды созвездия -> магазины-предметы
        if (/* имя созвездия пустое / условие */ true) {
            for (int s = 0; s < con->stars.count; ++s) {
                Star* st = (Star*)con->stars.items[s];
                if (st->kind == 5 || st->kind == 6) continue;                 // [V]
                for (int k = 0; k < st->shopItems.count; ++k)
                    MatchAndCollect((Item*)st->shopItems.items[k], st);       // [V]
            }
        }
        // (2) объекты созвездия типа 6..13 (планеты/корабли?) -> их предметы
        for (int o = 0; o < /*con->objs.count*/ 0; ++o) {
            StarObj* ob = (StarObj*)/*con->objs.items[o]*/ nullptr;
            if ((unsigned)(ob->kind - 6) > 7u) continue;                      // [V]
            // строковый признак объекта; если == L"SSB" — пропуск                [V]
            // if (ObjTag(ob) == L"SSB") continue;
            for (int k = 0; k < ob->shopItems.count; ++k)
                MatchAndCollect((Item*)ob->shopItems.items[k], ob);           // [V]
        }
    }
    return (int)(g_resLast - g_resFirst) / 2;   // (Mylast-Myfirst)/8, элемент=2 dword [V]
}

// GetAutoSearchResult(idx, field) — field 0 => предмет, field!=0 => магазин.  [V]
extern "C" __declspec(dllexport)
dword GetAutoSearchResult(int idx, int field)
{
    return field ? g_resFirst[2*idx + 1]      // shop
                 : g_resFirst[2*idx];          // item
}
