/***************************************
 * Name: dwDriveD2
 * Version: 1.0 Released
 * Editor: Aysi
 * Function: Drive DWDISPLAY D2
 * History: 2019.2.15: Created
 * ************************************/

#include "dwDriveD2.h"

u8 dwD2RecData[dwD2CacheSize];
u8 dwD2RecSta = 0, dwD2SendSta = 0, dwD2Lang = 0, dwD2RecDataBusy = 0;

/****************************
 * Function: Time process(use in timer function)
 * *************************/
void dwD2TimeProcess(void)
{
    static u8 i = 0, j = 0;

//if Rec package over time, this data cancel
#if dwD2RecOvertime > 0

    if (dwD2RecSta == 1)
    {
        if (++j >= dwD2RecOvertime)
        {
            dwD2RecSta = 2; //over time error
            j = 0;
        }
    }
    else
    {
        j = 0;
    }

#endif

//after send frame, need get respond
#if dwD2RespondOvertime > 0

    if (dwD2SendSta == 2)
    {
        if (++i >= dwD2RespondOvertime)
        {
            dwD2SendSta = 4; //over time error
            i = 0;
        }
    }
    else
    {
        i = 0;
    }

#endif
}

/****************************
 * Function: Send one frame
 * Parameter: i: data group address;
 *            p: data group quantity.
 * *************************/
void dwD2SendFrame(u8 *i, u8 p)
{
    dwD2SendSta = 1; //start send process
    uasrt2SendByte(dwD2HEAD >> 8);
    uasrt2SendByte((u8)dwD2HEAD);
    uasrt2SendByte(p);
    for (; p > 0; i++, p--)
    {
        uasrt2SendByte(*i);
    }

#if dwD2RespondOvertime > 0

    dwD2SendSta = 2; //wait respond or overtime
    while (dwD2SendSta == 2)
        ;

#else

    dwD2SendSta = 3; //Respond success

#endif
}

/****************************
 * Function: Process one frame
 * Parameter: i: input data;
 * *************************/
void dwD2RecFrame(u8 i)
{
    static u8 step = 0, len = 0;

    //Check protect
    if (dwD2RecDataBusy)
        return;

    //Over time, data del
    if (dwD2RecSta == 2)
    {
        len = 0;
        step = 0;
        dwD2RecSta = 0; //without rec
    }

    switch (step)
    {
    case 0:
    {
        if (i == (dwD2HEAD >> 8))
        {
            step++;
            dwD2RecSta = 1; //start rec frame
        }
        break;
    }
    case 1:
    {
        if (i == (dwD2HEAD & 0X00FF))
        {
            step++;
        }
        else
        { //head error
            step = 0;
            dwD2RecSta = 0; //without rec
        }
        break;
    }
    case 2:
    {
        dwD2RecData[0] = i;
        step++;
        break;
    }
    case 3:
    {
        len++;
        if (len >= dwD2CacheSize) //Length error
        {
            dwD2RecSta = 2;
            return;
        }
        dwD2RecData[len] = i;
        if (dwD2RecData[0] <= len)
        {
            step = 0;
            len = 0;
            dwD2RecSta = 3; //Rec OK

            if (dwD2SendSta == 2) //Send respond processes
            {
                dwD2SendSta = 3; //Success respond
            }
        }
        break;
    }
    default:
        break;
    }
}

//Get current Pic ID
static const u8 dwD2GetPicIDFrame[] = {0X83, 0X00, 0X14, 0X01};
u16 dwD2GetPicID(void)
{
    u8 i = 0;
    u16 j = 0;

    dwD2SendFrame((u8 *)&dwD2GetPicIDFrame[0], sizeof(dwD2GetPicIDFrame));
    while (dwD2RecSta != 3)
    {
        if (++i >= 5)
            return 0XFFFF; //Fail
        else
            delay_ms(2);
    }
    dwD2RecSta = 0;
    if ((dwD2RecData[3] == 0X14) && (dwD2RecData[4] == 0X01))
        return ((j + dwD2RecData[5]) << 8) + dwD2RecData[6];
    else
        return 0XFFFF; //Fail
}

//Check Display
u8 dwD2Check(void)
{
    u8 i = 0;

    delay_ms(1000);
    delay_ms(1000);
    for (; i < 100; i++)
    {
        if (dwD2GetPicID() != 0XFFFF)
            return 0; //Success
    }
    return 1; //Fail
}

//Reset
static const u8 dwD2ResetFrame[] = {0X82, 0X00, 0X04, 0X55, 0XAA, 0X5A, 0XA5};
void dwD2Rest(void)
{
    dwD2SendFrame((u8 *)&dwD2ResetFrame[0], sizeof(dwD2ResetFrame));
    delay_ms(1000); //wait ready
}

//Set BL
//i: Open Value, j: Close Value, k: Open time(value*4ms)
static u8 dwD2BLFrame[] = {0X82, 0X00, 0X82, 0X64, 0X00, 0X09, 0XC4};
void dwD2SetBL(u8 i, u8 j, u16 k)
{
    dwD2BLFrame[3] = i;
    dwD2BLFrame[4] = j;
    dwD2BLFrame[6] = (u8)k;
    k = k>>8;
    dwD2BLFrame[5] = (u8)k;
    dwD2SendFrame(&dwD2BLFrame[0], sizeof(dwD2BLFrame));
}

/*Page change*/
static u8 dwD2PicFrame[] = {0X82, 0X00, 0X84, 0X5A, 0X01, 0X00, 0X00};

//with language
void dwD2DisPicWithL(u8 i)
{
    dwD2PicFrame[6] = dwD2Lang + i;
    dwD2SendFrame(&dwD2PicFrame[0], sizeof(dwD2PicFrame));
}

//without language
void dwD2DisPicNoL(u8 i)
{
    dwD2PicFrame[6] = i;
    dwD2SendFrame(&dwD2PicFrame[0], sizeof(dwD2PicFrame));
}

/*display string*/
u8 dwD2CharFrame[8] = {0X82, 0X1B, 0X00, 0X31, 0X32, 0X2E, 0X42, 0X43};

//Display time 
void dwD2DisTime(u16 address, u16 timing)
{
    dwD2CharFrame[2] = (u8)address;
    dwD2CharFrame[1] = address>>8;
    dwD2CharFrame[3] = timing/600+'0';
    dwD2CharFrame[4] = timing/60%10+'0';
    dwD2CharFrame[5] = ':';
    dwD2CharFrame[6] = timing%60/10+'0';
    dwD2CharFrame[7] = timing%10+'0';
    dwD2SendFrame(&dwD2CharFrame[0], sizeof(dwD2CharFrame));
}

void dwD2DisFre(u16 address, u16 i)
{
    dwD2CharFrame[2] = (u8)address;
    dwD2CharFrame[1] = address>>8;
    dwD2CharFrame[3] = i/100+'0';
    dwD2CharFrame[4] = i/10%10+'0';
    dwD2CharFrame[5] = '.';
    dwD2CharFrame[6] = i%10+'0';
    dwD2CharFrame[7] = ' ';
    dwD2SendFrame(&dwD2CharFrame[0], sizeof(dwD2CharFrame));
}

//Display number
void dwD2DisNum(u16 address, u8 i)
{
    dwD2CharFrame[2] = (u8)address;
    dwD2CharFrame[1] = address>>8;
    dwD2CharFrame[3] = ' ';
    dwD2CharFrame[4] = ' ';
    dwD2CharFrame[5] = i%10+'0';
    dwD2CharFrame[6] = ' ';
    dwD2CharFrame[7] = ' ';
    dwD2SendFrame(&dwD2CharFrame[0], sizeof(dwD2CharFrame));
}

//Display char
void dwD2DisChar(u16 address, u8 i)
{
    dwD2CharFrame[2] = (u8)address;
    dwD2CharFrame[1] = address>>8;
    dwD2CharFrame[3] = ' ';
    dwD2CharFrame[4] = ' ';
    dwD2CharFrame[5] = i+'A';
    dwD2CharFrame[6] = ' ';
    dwD2CharFrame[7] = ' ';
    dwD2SendFrame(&dwD2CharFrame[0], sizeof(dwD2CharFrame));
}

//Display ICO
void dwD2DisICO(u16 address, u8 i)
{
    u8 dwD2ICOFrame[5];

    dwD2ICOFrame[0] = 0X82;
    dwD2ICOFrame[2] = (u8)address;
    dwD2ICOFrame[1] = address>>8;
    dwD2ICOFrame[3] = 0X00;
    dwD2ICOFrame[4] = i;
 
    dwD2SendFrame(&dwD2ICOFrame[0], sizeof(dwD2ICOFrame));
}

/*Key*/
static _DwD2KeyListen _dwKeyListen[30];
static u8 keyNum = 0;

//Load out Key
void dwD2CancelKey(void)
{
    keyNum = 0;
}

//Key load
void dwD2ListenKey(void (*press)(void), const u16 btn)
{
    if (keyNum < 30)
    {
        _dwKeyListen[keyNum].pressHandle = press;
        _dwKeyListen[keyNum].button = btn;
        keyNum++;
    }
}

//key process
void dwD2Handler(void)
{
    u16 j = 0;
    u8 i = 0;

    // Press
    if (dwD2RecSta == 3)
    {
        dwD2RecSta = 0;      //Data processed finish, clear
        dwD2RecDataBusy = 1; //Protect data before processing
    }
    else
    {
        return;
    }

    if (((dwD2RecData[2] == (KEY_ADDRESS >> 8)) && (dwD2RecData[3] == (KEY_ADDRESS & 0X00FF)))||
        ((dwD2RecData[2] == (0X2101 >> 8)) && (dwD2RecData[3] == (0X2101 & 0X00FF))))
    {
        uasrt1SendByte(dwD2RecData[6]);
        //check key code
        j = ((j + dwD2RecData[5]) << 8) + dwD2RecData[6];
        for (; i <= keyNum; i++)
        {
            if (j == _dwKeyListen[i].button)
                break;
        }

        if (i <= keyNum) //key valid
        {
            if (_dwKeyListen[i].pressHandle != 0)
            { //not null
                _dwKeyListen[i].pressHandle();
            }
        }
    }

    dwD2RecDataBusy = 0; //Protect data cancel
}
