/**************************************************************************/
/* LabWindows/CVI User Interface Resource (UIR) Include File              */
/*                                                                        */
/* WARNING: Do not add to, delete from, or otherwise modify the contents  */
/*          of this include file.                                         */
/**************************************************************************/

#include <userint.h>

#ifdef __cplusplus
    extern "C" {
#endif

     /* Panels and Controls: */

#define  MAIN_PANEL                       1
#define  MAIN_PANEL_TIMER                 2       /* control type: timer, callback function: TimerCallback */
#define  MAIN_PANEL_TABS                  3       /* control type: tab, callback function: (none) */

     /* tab page panel controls */
#define  FILTER_FILTER_TYPE               2       /* control type: ring, callback function: (none) */
#define  FILTER_FILTER_ORDER              3       /* control type: numeric, callback function: (none) */
#define  FILTER_FILTER_LOW_CUTOFF         4       /* control type: numeric, callback function: (none) */
#define  FILTER_FILTER_HIGH_CUTOFF        5       /* control type: numeric, callback function: (none) */
#define  FILTER_FILTER_CUTOFF             6       /* control type: numeric, callback function: (none) */
#define  FILTER_FILTER_APPLY              7       /* control type: command, callback function: FilterCallback */
#define  FILTER_FILTER_OUTPUT_SPECTRU     8       /* control type: graph, callback function: (none) */
#define  FILTER_FILTER_INPUT_SPECTRUM     9       /* control type: graph, callback function: (none) */
#define  FILTER_FILTER_OUTPUT_GRAPH       10      /* control type: graph, callback function: (none) */
#define  FILTER_FILTER_INPUT_GRAPH        11      /* control type: graph, callback function: (none) */
#define  FILTER_FILTER_EXPORT_EXCEL       12      /* control type: command, callback function: ExportExcelCallback */
#define  FILTER_FILTER_SAVE_DATA          13      /* control type: command, callback function: SaveDataCallback */
#define  FILTER_FILTER_INPUT_S            14      /* control type: ring, callback function: (none) */

     /* tab page panel controls */
#define  SCOPE_QUIT_2                     2       /* control type: command, callback function: QUIT */
#define  SCOPE_MT_GEN2_RMS                3       /* control type: numeric, callback function: (none) */
#define  SCOPE_MT_GEN2_PERIOD             4       /* control type: numeric, callback function: (none) */
#define  SCOPE_MT_GEN2_MEAS_FREQ          5       /* control type: numeric, callback function: (none) */
#define  SCOPE_MT_GEN2_VPP                6       /* control type: numeric, callback function: (none) */
#define  SCOPE_volt_6                     7       /* control type: textMsg, callback function: (none) */
#define  SCOPE_MEAS_RMS                   8       /* control type: numeric, callback function: (none) */
#define  SCOPE_MEAS_PERIOD                9       /* control type: numeric, callback function: (none) */
#define  SCOPE_hertz2_2                   10      /* control type: textMsg, callback function: (none) */
#define  SCOPE_period_2                   11      /* control type: textMsg, callback function: (none) */
#define  SCOPE_MEAS_FREQ                  12      /* control type: numeric, callback function: (none) */
#define  SCOPE_MEAS_VPP                   13      /* control type: numeric, callback function: (none) */
#define  SCOPE_volt_3                     14      /* control type: textMsg, callback function: (none) */
#define  SCOPE_MT_GEN2_WAVEFORM           15      /* control type: ring, callback function: (none) */
#define  SCOPE_GEN_WAVEFORM               16      /* control type: ring, callback function: (none) */
#define  SCOPE_hertz2                     17      /* control type: textMsg, callback function: (none) */
#define  SCOPE_period                     18      /* control type: textMsg, callback function: (none) */
#define  SCOPE_VOLT_6                     19      /* control type: textMsg, callback function: (none) */
#define  SCOPE_HERTZ_2                    20      /* control type: textMsg, callback function: (none) */
#define  SCOPE_MT_GEN2_PHASE              21      /* control type: numeric, callback function: (none) */
#define  SCOPE_degres_2                   22      /* control type: textMsg, callback function: (none) */
#define  SCOPE_MT_GEN2_OFFSET             23      /* control type: numeric, callback function: (none) */
#define  SCOPE_volt_4                     24      /* control type: textMsg, callback function: (none) */
#define  SCOPE_volt_5                     25      /* control type: textMsg, callback function: (none) */
#define  SCOPE_MT_GEN2_OUTPUT             26      /* control type: binary, callback function: OutputCallback */
#define  SCOPE_VOLT_5                     27      /* control type: textMsg, callback function: (none) */
#define  SCOPE_MT_COMBINED_GRAPH          28      /* control type: graph, callback function: (none) */
#define  SCOPE_MT_GEN2_GRAPH              29      /* control type: graph, callback function: (none) */
#define  SCOPE_HERTZ                      30      /* control type: textMsg, callback function: (none) */
#define  SCOPE_GEN_PHASE                  31      /* control type: numeric, callback function: (none) */
#define  SCOPE_degres                     32      /* control type: textMsg, callback function: (none) */
#define  SCOPE_GEN_OFFSET                 33      /* control type: numeric, callback function: (none) */
#define  SCOPE_volt                       34      /* control type: textMsg, callback function: (none) */
#define  SCOPE_volt_2                     35      /* control type: textMsg, callback function: (none) */
#define  SCOPE_GEN_OUTPUT                 36      /* control type: binary, callback function: OutputCallback */
#define  SCOPE_SCOPE_GRAPH                37      /* control type: graph, callback function: (none) */
#define  SCOPE_MT_GENERATE                38      /* control type: command, callback function: GenerateMultiThreadCallback */
#define  SCOPE_MT_GEN2_LED                39      /* control type: LED, callback function: (none) */
#define  SCOPE_MT_GEN1_LED                40      /* control type: LED, callback function: (none) */
#define  SCOPE_SCOPE_MT_THREAD2_TIME      41      /* control type: numeric, callback function: (none) */
#define  SCOPE_SCOPE_MT_THREAD1_TIME      42      /* control type: numeric, callback function: (none) */
#define  SCOPE_SCOPE_RESET_ALL            43      /* control type: command, callback function: ResetAllCallback */
#define  SCOPE_MT_GEN2_FREQ               44      /* control type: scale, callback function: (none) */
#define  SCOPE_GEN_FREQ                   45      /* control type: scale, callback function: (none) */
#define  SCOPE_MT_GEN2_AMP                46      /* control type: scale, callback function: (none) */
#define  SCOPE_GEN_AMP                    47      /* control type: scale, callback function: (none) */
#define  SCOPE_SCOPE_EXT_VOLTAGE          48      /* control type: numeric, callback function: (none) */
#define  SCOPE_SCOPE_EXT_COM_PORT         49      /* control type: numeric, callback function: (none) */
#define  SCOPE_SCOPE_EXT_CONNECT          50      /* control type: command, callback function: ExternalConnectCallback */
#define  SCOPE_DECORATION                 51      /* control type: deco, callback function: (none) */
#define  SCOPE_TEXTMSG                    52      /* control type: textMsg, callback function: (none) */
#define  SCOPE_DECORATION_2               53      /* control type: deco, callback function: (none) */

     /* tab page panel controls */
#define  SPEC_SPEC_GRAPH                  2       /* control type: graph, callback function: (none) */
#define  SPEC_HARMONICS                   3       /* control type: numeric, callback function: (none) */
#define  SPEC_PEAK_MAG                    4       /* control type: numeric, callback function: (none) */
#define  SPEC_PEAK_FREQ                   5       /* control type: numeric, callback function: (none) */
#define  SPEC_SPEC_MODE                   6       /* control type: ring, callback function: (none) */
#define  SPEC_SPEC_BODE_DEN_TABLE         7       /* control type: table, callback function: (none) */
#define  SPEC_SPEC_BODE_NUM_TABLE         8       /* control type: table, callback function: (none) */
#define  SPEC_SPEC_BODE_GENERATE          9       /* control type: command, callback function: BodeGenerateCallback */
#define  SPEC_SPEC_BODE_PHASE_GRAPH       10      /* control type: graph, callback function: (none) */
#define  SPEC_SPEC_BODE_MAG_GRAPH         11      /* control type: graph, callback function: (none) */
#define  SPEC_DECORATION_2                12      /* control type: deco, callback function: (none) */
#define  SPEC_PICTURE                     13      /* control type: picture, callback function: (none) */
#define  SPEC_DECORATION                  14      /* control type: deco, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK BodeGenerateCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ExportExcelCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ExternalConnectCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK FilterCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK GenerateMultiThreadCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK OutputCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK QUIT(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ResetAllCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK SaveDataCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK TimerCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif