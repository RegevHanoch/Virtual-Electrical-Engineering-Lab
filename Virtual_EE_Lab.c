#include <ansi_c.h>
#include <cvirte.h>
#include <userint.h>
#include <analysis.h>
#include <rs232.h>

#include "Excel.h"
#include <utility.h>
#include <formatio.h>
#include <math.h>
#include "Virtual_EE_Lab.h"

#define PI 3.14159265358979323846
#define N 1024
#define BODE_COEFFS 8
#define BODE_POINTS 500
#define ARDUINO_SAMPLE_RATE 100.0

static int mainPanel;
static int scopePanel;
static int specPanel;
static int filterPanel;

static double signalData[N];
static double filteredData[N];

static double sampleRate = 0.0;
static double sampleRate2 = 0.0;
static double combinedSampleRate = 0.0;

static double signal1Data[N];
static double signal2Data[N];
static double combinedSignal[N];

static double filterInputData[N];
static double filterInputSampleRate = 0.0;
static int filterInputSource = 0;

static CmtThreadFunctionID threadID1 = 0;
static CmtThreadFunctionID threadID2 = 0;

static int ledBlinkState = 0;
static double lastLedBlinkTime = 0.0;

static double thread1TotalTime = 0.0;
static double thread2TotalTime = 0.0;

static double thread1LastTime = 0.0;
static double thread2LastTime = 0.0;

static int previousOutput1 = 0;
static int previousOutput2 = 0;

static int arduinoConnected = 0;
static int arduinoComPort = 0;
static char arduinoLineBuffer[64];
static int arduinoLineLength = 0;
static int arduinoSampleCount = 0;

typedef struct
{
    int waveform;
    double frequency;
    double amplitude;
    double phase;
    double offset;
    double sampleRate;
    double *output;
} SignalThreadData;

void UpdateSignal(void);
void UpdateSignal2(void);
void UpdateSpectrum(void);
void AddArduinoSample(double voltage);
void UpdateArduinoScope(void);
int CVICALLBACK SignalGeneratorThread (void *functionData);
int CVICALLBACK GenerateMultiThreadCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int CVICALLBACK ResetAllCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int ReadBodePolynomial (int tableControl, double coefficients[]);
static int PrepareBodePolynomial (double source[], double destination[]);
static void EvaluateBodePolynomial (double coefficients[], int count, double omega, double *realPart, double *imagPart);
static void FindBodeFrequencyRange (double numerator[], int numeratorCount, double denominator[], int denominatorCount, double *startFrequency, double *stopFrequency);
static void UnwrapBodePhase (double phase[], int count);
int CVICALLBACK BodeGenerateCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int CVICALLBACK ExternalConnectCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


static int ReadBodePolynomial (int tableControl, double coefficients[])
{
    int i;
    int status;

    for (i = 0; i < BODE_COEFFS; i++)
    {
        status = GetTableCellVal (specPanel, tableControl, MakePoint (2, i + 1), &coefficients[i]);

        if (status < 0)
            return -1;
    }

    return 0;
}

static int PrepareBodePolynomial (double source[], double destination[])
{
    int first;
    int i;
    int count;

    first = 0;

    while (first < BODE_COEFFS && fabs (source[first]) < 1e-15)
        first++;

    if (first == BODE_COEFFS)
        return 0;

    count = BODE_COEFFS - first;

    for (i = 0; i < count; i++)
        destination[i] = source[first + i];

    return count;
}

static void EvaluateBodePolynomial (double coefficients[], int count, double omega, double *realPart, double *imagPart)
{
    int i;
    double oldReal;
    double oldImag;

    *realPart = coefficients[0];
    *imagPart = 0.0;

    for (i = 1; i < count; i++)
    {
        oldReal = *realPart;
        oldImag = *imagPart;

        *realPart = -omega * oldImag + coefficients[i];
        *imagPart = omega * oldReal;
    }
}

static void FindBodeFrequencyRange (double numerator[], int numeratorCount, double denominator[], int denominatorCount, double *startFrequency, double *stopFrequency)
{
    ComplexNum numeratorRoots[BODE_COEFFS - 1];
    ComplexNum denominatorRoots[BODE_COEFFS - 1];
    double rootMagnitude;
    double frequency;
    double minimumFrequency;
    double maximumFrequency;
    int numeratorStatus;
    int denominatorStatus;
    int i;
    int foundFrequency;

    minimumFrequency = 0.0;
    maximumFrequency = 0.0;
    foundFrequency = 0;
    numeratorStatus = 0;
    denominatorStatus = 0;

    if (numeratorCount > 1)
        numeratorStatus = CxPolyRoots (numerator, numeratorCount, numeratorRoots);

    if (denominatorCount > 1)
        denominatorStatus = CxPolyRoots (denominator, denominatorCount, denominatorRoots);

    if (numeratorCount > 1 && numeratorStatus >= 0)
    {
        for (i = 0; i < numeratorCount - 1; i++)
        {
            rootMagnitude = sqrt (numeratorRoots[i].real * numeratorRoots[i].real +
                                  numeratorRoots[i].imaginary * numeratorRoots[i].imaginary);

            if (rootMagnitude > 1e-12)
            {
                frequency = rootMagnitude;

                if (!foundFrequency)
                {
                    minimumFrequency = frequency;
                    maximumFrequency = frequency;
                    foundFrequency = 1;
                }
                else
                {
                    if (frequency < minimumFrequency)
                        minimumFrequency = frequency;

                    if (frequency > maximumFrequency)
                        maximumFrequency = frequency;
                }
            }
        }
    }

    if (denominatorCount > 1 && denominatorStatus >= 0)
    {
        for (i = 0; i < denominatorCount - 1; i++)
        {
            rootMagnitude = sqrt (denominatorRoots[i].real * denominatorRoots[i].real +
                                  denominatorRoots[i].imaginary * denominatorRoots[i].imaginary);

            if (rootMagnitude > 1e-12)
            {
                frequency = rootMagnitude;

                if (!foundFrequency)
                {
                    minimumFrequency = frequency;
                    maximumFrequency = frequency;
                    foundFrequency = 1;
                }
                else
                {
                    if (frequency < minimumFrequency)
                        minimumFrequency = frequency;

                    if (frequency > maximumFrequency)
                        maximumFrequency = frequency;
                }
            }
        }
    }

    if (!foundFrequency)
    {
        *startFrequency = 0.1;
        *stopFrequency = 10000.0;
        return;
    }

    *startFrequency = minimumFrequency / 100.0;
    *stopFrequency = maximumFrequency * 100.0;

    if (*startFrequency < 0.001)
        *startFrequency = 0.001;

    if (*stopFrequency > 1.0e9)
        *stopFrequency = 1.0e9;

    if (*stopFrequency / *startFrequency < 100.0)
    {
        *startFrequency = *startFrequency / 10.0;
        *stopFrequency = *stopFrequency * 10.0;
    }

    *startFrequency = pow (10.0, floor (log10 (*startFrequency)));
    *stopFrequency = pow (10.0, ceil (log10 (*stopFrequency)));
}

static void UnwrapBodePhase (double phase[], int count)
{
    int i;
    double difference;

    for (i = 1; i < count; i++)
    {
        difference = phase[i] - phase[i - 1];

        while (difference > 180.0)
        {
            phase[i] -= 360.0;
            difference = phase[i] - phase[i - 1];
        }

        while (difference < -180.0)
        {
            phase[i] += 360.0;
            difference = phase[i] - phase[i - 1];
        }
    }
}

int CVICALLBACK BodeGenerateCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    double numeratorTable[BODE_COEFFS];
    double denominatorTable[BODE_COEFFS];
    double numerator[BODE_COEFFS];
    double denominator[BODE_COEFFS];
    double frequency[BODE_POINTS];
    double magnitude[BODE_POINTS];
    double phase[BODE_POINTS];
    double numeratorReal;
    double numeratorImag;
    double denominatorReal;
    double denominatorImag;
    double transferReal;
    double transferImag;
    double denominatorMagnitudeSquared;
    double transferMagnitude;
    double startFrequency;
    double stopFrequency;
    double logStart;
    double logStop;
    double omega;
    int numeratorCount;
    int denominatorCount;
    int i;

    if (event != EVENT_COMMIT)
        return 0;

    if (ReadBodePolynomial (SPEC_SPEC_BODE_NUM_TABLE, numeratorTable) < 0)
    {
        MessagePopup ("Bode Plot", "Unable to read numerator coefficients.");
        return 0;
    }

    if (ReadBodePolynomial (SPEC_SPEC_BODE_DEN_TABLE, denominatorTable) < 0)
    {
        MessagePopup ("Bode Plot", "Unable to read denominator coefficients.");
        return 0;
    }

    numeratorCount = PrepareBodePolynomial (numeratorTable, numerator);
    denominatorCount = PrepareBodePolynomial (denominatorTable, denominator);

    if (numeratorCount == 0)
    {
        MessagePopup ("Bode Plot", "Numerator cannot contain only zeros.");
        return 0;
    }

    if (denominatorCount == 0)
    {
        MessagePopup ("Bode Plot", "Denominator cannot contain only zeros.");
        return 0;
    }

    FindBodeFrequencyRange (numerator, numeratorCount, denominator, denominatorCount, &startFrequency, &stopFrequency);

    logStart = log10 (startFrequency);
    logStop = log10 (stopFrequency);

    for (i = 0; i < BODE_POINTS; i++)
    {
        frequency[i] = pow (10.0, logStart + (logStop - logStart) * ((double)i / (double)(BODE_POINTS - 1)));
        omega = frequency[i];

        EvaluateBodePolynomial (numerator, numeratorCount, omega, &numeratorReal, &numeratorImag);
        EvaluateBodePolynomial (denominator, denominatorCount, omega, &denominatorReal, &denominatorImag);

        denominatorMagnitudeSquared = denominatorReal * denominatorReal + denominatorImag * denominatorImag;

        if (denominatorMagnitudeSquared < 1e-30)
        {
            magnitude[i] = 300.0;
            phase[i] = 0.0;
        }
        else
        {
            transferReal = (numeratorReal * denominatorReal + numeratorImag * denominatorImag) / denominatorMagnitudeSquared;
            transferImag = (numeratorImag * denominatorReal - numeratorReal * denominatorImag) / denominatorMagnitudeSquared;

            transferMagnitude = sqrt (transferReal * transferReal + transferImag * transferImag);

            if (transferMagnitude < 1e-15)
                transferMagnitude = 1e-15;

            magnitude[i] = 20.0 * log10 (transferMagnitude);
            phase[i] = atan2 (transferImag, transferReal) * 180.0 / PI;
        }
    }

    UnwrapBodePhase (phase, BODE_POINTS);

    DeleteGraphPlot (specPanel, SPEC_SPEC_BODE_MAG_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    DeleteGraphPlot (specPanel, SPEC_SPEC_BODE_PHASE_GRAPH, -1, VAL_IMMEDIATE_DRAW);

    SetCtrlAttribute (specPanel, SPEC_SPEC_BODE_MAG_GRAPH, ATTR_XMAP_MODE, VAL_LOG);
    SetCtrlAttribute (specPanel, SPEC_SPEC_BODE_PHASE_GRAPH, ATTR_XMAP_MODE, VAL_LOG);

    SetAxisScalingMode (specPanel, SPEC_SPEC_BODE_MAG_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, startFrequency, stopFrequency);
    SetAxisScalingMode (specPanel, SPEC_SPEC_BODE_MAG_GRAPH, VAL_LEFT_YAXIS, VAL_AUTOSCALE, 0.0, 0.0);
    SetAxisScalingMode (specPanel, SPEC_SPEC_BODE_PHASE_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, startFrequency, stopFrequency);
    SetAxisScalingMode (specPanel, SPEC_SPEC_BODE_PHASE_GRAPH, VAL_LEFT_YAXIS, VAL_AUTOSCALE, 0.0, 0.0);

    PlotXY (specPanel, SPEC_SPEC_BODE_MAG_GRAPH, frequency, magnitude, BODE_POINTS, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_EMPTY_SQUARE, VAL_SOLID, 1, VAL_BLUE);
    PlotXY (specPanel, SPEC_SPEC_BODE_PHASE_GRAPH, frequency, phase, BODE_POINTS, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_EMPTY_SQUARE, VAL_SOLID, 1, VAL_RED);

    return 0;
}

int main (int argc, char *argv[])
{
    CA_InitActiveXThreadStyleForCurrentThread (0, COINIT_APARTMENTTHREADED);

    if (InitCVIRTE (0, argv, 0) == 0)
        return -1;

    if ((mainPanel = LoadPanel (0, "Virtual_EE_Lab.uir", MAIN_PANEL)) < 0)
        return -1;

    GetPanelHandleFromTabPage (mainPanel, MAIN_PANEL_TABS, 0, &scopePanel);
    GetPanelHandleFromTabPage (mainPanel, MAIN_PANEL_TABS, 1, &specPanel);
    GetPanelHandleFromTabPage (mainPanel, MAIN_PANEL_TABS, 2, &filterPanel);

    lastLedBlinkTime = Timer();

    thread1TotalTime = 0.0;
    thread2TotalTime = 0.0;

    thread1LastTime = Timer();
    thread2LastTime = Timer();

    previousOutput1 = 0;
    previousOutput2 = 0;
	

	

    SetCtrlVal (scopePanel, SCOPE_SCOPE_MT_THREAD1_TIME, 0.0);
    SetCtrlVal (scopePanel, SCOPE_SCOPE_MT_THREAD2_TIME, 0.0);

    DisplayPanel (mainPanel);

    RunUserInterface ();

    DiscardPanel (mainPanel);

    return 0;
}

void AddArduinoSample(double voltage)
{
    int i;

    if (arduinoSampleCount < N)
    {
        signalData[arduinoSampleCount] = voltage;
        arduinoSampleCount++;
    }
    else
    {
        for (i = 1; i < N; i++)
            signalData[i - 1] = signalData[i];

        signalData[N - 1] = voltage;
    }

    sampleRate = ARDUINO_SAMPLE_RATE;
}

void UpdateArduinoScope(void)
{
    int i;
    int crossingCount;
    int firstCrossing;
    int lastCrossing;

    double x[N];
    double minimum;
    double maximum;
    double mean;
    double rms;
    double sum;
    double sumSquares;
    double margin;
    double xMax;
    double frequency;
    double period;

    if (!arduinoConnected || arduinoSampleCount <= 0)
        return;

    minimum = signalData[0];
    maximum = signalData[0];
    sum = 0.0;
    sumSquares = 0.0;

    for (i = 0; i < arduinoSampleCount; i++)
    {
        x[i] = i * 1000.0 / ARDUINO_SAMPLE_RATE;

        if (signalData[i] < minimum)
            minimum = signalData[i];

        if (signalData[i] > maximum)
            maximum = signalData[i];

        sum += signalData[i];
        sumSquares += signalData[i] * signalData[i];
    }

    mean = sum / arduinoSampleCount;
    rms = sqrt (sumSquares / arduinoSampleCount);

    crossingCount = 0;
    firstCrossing = -1;
    lastCrossing = -1;

    for (i = 1; i < arduinoSampleCount; i++)
    {
        if (signalData[i - 1] < mean && signalData[i] >= mean)
        {
            if (firstCrossing < 0)
                firstCrossing = i;

            lastCrossing = i;
            crossingCount++;
        }
    }

    frequency = 0.0;

    if (crossingCount >= 2 && lastCrossing > firstCrossing)
        frequency = (crossingCount - 1) * ARDUINO_SAMPLE_RATE / (lastCrossing - firstCrossing);

    if (frequency > 0.0)
        period = 1000.0 / frequency;
    else
        period = 0.0;

    margin = (maximum - minimum) * 0.2;

    if (margin < 0.2)
        margin = 0.2;

    xMax = x[arduinoSampleCount - 1];

    if (xMax <= 0.0)
        xMax = 1000.0 / ARDUINO_SAMPLE_RATE;

    DeleteGraphPlot (scopePanel, SCOPE_SCOPE_GRAPH, -1, VAL_IMMEDIATE_DRAW);

    PlotXY (scopePanel, SCOPE_SCOPE_GRAPH, x, signalData, arduinoSampleCount, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, VAL_GREEN);

    SetAxisScalingMode (scopePanel, SCOPE_SCOPE_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0.0, xMax);
    SetAxisScalingMode (scopePanel, SCOPE_SCOPE_GRAPH, VAL_LEFT_YAXIS, VAL_MANUAL, minimum - margin, maximum + margin);

    SetCtrlVal (scopePanel, SCOPE_MEAS_VPP, maximum - minimum);
    SetCtrlVal (scopePanel, SCOPE_MEAS_RMS, rms);
    SetCtrlVal (scopePanel, SCOPE_MEAS_FREQ, frequency);
    SetCtrlVal (scopePanel, SCOPE_MEAS_PERIOD, period);

    UpdateSpectrum();
}

void UpdateSignal(void)
{
    int outputState;
    int waveform;
    int i;

    double frequency;
    double amplitude;
    double phase;
    double offset;

    double x[N];

    double dt;
    double t;
    double phaseRad;
    double angle;

    double yMin;
    double yMax;
    double yMargin;

    double displayTime;
    double xMax;

    GetCtrlVal (scopePanel, SCOPE_GEN_OUTPUT, &outputState);

    if (outputState == 1)
    {
        GetCtrlVal (scopePanel, SCOPE_GEN_WAVEFORM, &waveform);
        GetCtrlVal (scopePanel, SCOPE_GEN_FREQ, &frequency);
        GetCtrlVal (scopePanel, SCOPE_GEN_AMP, &amplitude);
        GetCtrlVal (scopePanel, SCOPE_GEN_PHASE, &phase);
        GetCtrlVal (scopePanel, SCOPE_GEN_OFFSET, &offset);

        if (amplitude < 0.0)
            amplitude = -amplitude;

        yMargin = amplitude * 0.2;

        if (yMargin < 1.0)
            yMargin = 1.0;

        yMin = offset - amplitude - yMargin;
        yMax = offset + amplitude + yMargin;

        SetAxisScalingMode (scopePanel, SCOPE_SCOPE_GRAPH, VAL_LEFT_YAXIS, VAL_MANUAL, yMin, yMax);

        if (frequency > 0.0)
        {
            displayTime = 5.0 / frequency;
            xMax = displayTime * 1000.0;
        }
        else
        {
            displayTime = 0.005;
            xMax = 5.0;
        }

        SetAxisScalingMode (scopePanel, SCOPE_SCOPE_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0.0, xMax);

        dt = displayTime / N;

        sampleRate = 1.0 / dt;

        phaseRad = phase * PI / 180.0;

        for (i = 0; i < N; i++)
        {
            t = i * dt;

            x[i] = t * 1000.0;

            angle = 2.0 * PI * frequency * t + phaseRad;

            if (waveform == 0)
                signalData[i] = amplitude * sin(angle) + offset;
            else if (waveform == 1)
                signalData[i] = (sin(angle) >= 0.0 ? amplitude : -amplitude) + offset;
            else if (waveform == 2)
                signalData[i] = amplitude * (2.0 / PI) * asin(sin(angle)) + offset;
            else
                signalData[i] = 0.0;
        }

        DeleteGraphPlot (scopePanel, SCOPE_SCOPE_GRAPH, -1, VAL_IMMEDIATE_DRAW);

        PlotXY (scopePanel, SCOPE_SCOPE_GRAPH, x, signalData, N, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, VAL_GREEN);

        SetCtrlVal (scopePanel, SCOPE_MEAS_VPP, 2.0 * amplitude);
        SetCtrlVal (scopePanel, SCOPE_MEAS_FREQ, frequency);

        if (frequency > 0.0)
            SetCtrlVal (scopePanel, SCOPE_MEAS_PERIOD, 1000.0 / frequency);
        else
            SetCtrlVal (scopePanel, SCOPE_MEAS_PERIOD, 0.0);

        if (waveform == 0)
            SetCtrlVal (scopePanel, SCOPE_MEAS_RMS, amplitude / sqrt(2.0));
        else if (waveform == 1)
            SetCtrlVal (scopePanel, SCOPE_MEAS_RMS, amplitude);
        else if (waveform == 2)
            SetCtrlVal (scopePanel, SCOPE_MEAS_RMS, amplitude / sqrt(3.0));
        else
            SetCtrlVal (scopePanel, SCOPE_MEAS_RMS, 0.0);
    }
    else
    {
        sampleRate = 0.0;

        for (i = 0; i < N; i++)
            signalData[i] = 0.0;

        DeleteGraphPlot (scopePanel, SCOPE_SCOPE_GRAPH, -1, VAL_IMMEDIATE_DRAW);

        SetCtrlVal (scopePanel, SCOPE_MEAS_VPP, 0.0);
        SetCtrlVal (scopePanel, SCOPE_MEAS_RMS, 0.0);
        SetCtrlVal (scopePanel, SCOPE_MEAS_FREQ, 0.0);
        SetCtrlVal (scopePanel, SCOPE_MEAS_PERIOD, 0.0);
    }

    UpdateSpectrum();
}

void UpdateSignal2(void)
{
    int outputState;
    int waveform;
    int i;

    double frequency;
    double amplitude;
    double phase;
    double offset;

    double x[N];

    double dt;
    double t;
    double phaseRad;
    double angle;

    double yMin;
    double yMax;
    double yMargin;

    double displayTime;
    double xMax;

    GetCtrlVal (scopePanel, SCOPE_MT_GEN2_OUTPUT, &outputState);

    if (outputState == 1)
    {
        GetCtrlVal (scopePanel, SCOPE_MT_GEN2_WAVEFORM, &waveform);
        GetCtrlVal (scopePanel, SCOPE_MT_GEN2_FREQ, &frequency);
        GetCtrlVal (scopePanel, SCOPE_MT_GEN2_AMP, &amplitude);
        GetCtrlVal (scopePanel, SCOPE_MT_GEN2_PHASE, &phase);
        GetCtrlVal (scopePanel, SCOPE_MT_GEN2_OFFSET, &offset);

        if (amplitude < 0.0)
            amplitude = -amplitude;

        if (frequency > 0.0)
        {
            displayTime = 5.0 / frequency;
            xMax = displayTime * 1000.0;
        }
        else
        {
            displayTime = 0.005;
            xMax = 5.0;
        }

        dt = displayTime / N;

        sampleRate2 = 1.0 / dt;

        phaseRad = phase * PI / 180.0;

        for (i = 0; i < N; i++)
        {
            t = i * dt;

            x[i] = t * 1000.0;

            angle = 2.0 * PI * frequency * t + phaseRad;

            if (waveform == 0)
                signal2Data[i] = amplitude * sin(angle) + offset;
            else if (waveform == 1)
                signal2Data[i] = (sin(angle) >= 0.0 ? amplitude : -amplitude) + offset;
            else if (waveform == 2)
                signal2Data[i] = amplitude * (2.0 / PI) * asin(sin(angle)) + offset;
            else
                signal2Data[i] = 0.0;
        }

        yMargin = amplitude * 0.2;

        if (yMargin < 1.0)
            yMargin = 1.0;

        yMin = offset - amplitude - yMargin;
        yMax = offset + amplitude + yMargin;

        DeleteGraphPlot (scopePanel, SCOPE_MT_GEN2_GRAPH, -1, VAL_IMMEDIATE_DRAW);

        PlotXY (scopePanel, SCOPE_MT_GEN2_GRAPH, x, signal2Data, N, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, VAL_GREEN);

        SetAxisScalingMode (scopePanel, SCOPE_MT_GEN2_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0.0, xMax);

        SetAxisScalingMode (scopePanel, SCOPE_MT_GEN2_GRAPH, VAL_LEFT_YAXIS, VAL_MANUAL, yMin, yMax);
    }
    else
    {
        sampleRate2 = 0.0;

        for (i = 0; i < N; i++)
            signal2Data[i] = 0.0;

        DeleteGraphPlot (scopePanel, SCOPE_MT_GEN2_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    }
}

void UpdateSpectrum(void)
{
    int outputState;
    int mode;
    int waveform;
    int i;
    int harmonicCount;
    int status;
    int validSamples;

    double real[N] = {0};
    double imag[N] = {0};

    double freq;
    double magnitude;
    double maxMagnitude;
    double peakFrequency;
    double threshold;
    double xRange;

    double xLine[2] = {0};
    double yLine[2] = {0};

    double xPoint[1] = {0};
    double yPoint[1] = {0};

    GetCtrlVal (scopePanel, SCOPE_GEN_OUTPUT, &outputState);
    GetCtrlVal (scopePanel, SCOPE_GEN_WAVEFORM, &waveform);
    GetCtrlVal (specPanel, SPEC_SPEC_MODE, &mode);

    DeleteGraphPlot (specPanel, SPEC_SPEC_GRAPH, -1, VAL_IMMEDIATE_DRAW);

    if (arduinoConnected)
    {
        if (sampleRate <= 0.0 || arduinoSampleCount < 8)
        {
            SetCtrlVal (specPanel, SPEC_PEAK_FREQ, 0.0);
            SetCtrlVal (specPanel, SPEC_PEAK_MAG, 0.0);
            SetCtrlVal (specPanel, SPEC_HARMONICS, 0.0);
            return;
        }

        validSamples = arduinoSampleCount;
    }
    else
    {
        if (outputState == 0 || sampleRate <= 0.0)
        {
            SetCtrlVal (specPanel, SPEC_PEAK_FREQ, 0.0);
            SetCtrlVal (specPanel, SPEC_PEAK_MAG, 0.0);
            SetCtrlVal (specPanel, SPEC_HARMONICS, 0.0);
            return;
        }

        validSamples = N;
    }

    for (i = 0; i < N; i++)
    {
        if (i < validSamples)
            real[i] = signalData[i];
        else
            real[i] = 0.0;

        imag[i] = 0.0;
    }

    status = FFT (real, imag, N);

    if (status < 0)
        return;

    maxMagnitude = 0.0;
    peakFrequency = 0.0;

    for (i = 1; i < N / 2; i++)
    {
        magnitude = sqrt(real[i] * real[i] + imag[i] * imag[i]) * 2.0 / N;

        if (magnitude > maxMagnitude)
        {
            maxMagnitude = magnitude;
            peakFrequency = i * sampleRate / N;
        }
    }

    threshold = maxMagnitude * 0.02;
    harmonicCount = 0;

    if (arduinoConnected)
        xRange = sampleRate / 2.0;
    else if (waveform == 0)
        xRange = peakFrequency * 2.0;
    else
        xRange = peakFrequency * 11.0;

    if (xRange <= 0.0)
        xRange = 1000.0;

    if (mode == 0)
    {
        for (i = 1; i < N / 2; i++)
        {
            freq = i * sampleRate / N;
            magnitude = sqrt(real[i] * real[i] + imag[i] * imag[i]) * 2.0 / N;

            if (magnitude >= threshold && freq <= xRange)
            {
                xLine[0] = freq;
                xLine[1] = freq;
                yLine[0] = 0.0;
                yLine[1] = magnitude;

                PlotXY (specPanel, SPEC_SPEC_GRAPH, xLine, yLine, 2, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 4, VAL_GREEN);

                xPoint[0] = freq;
                yPoint[0] = magnitude;

                PlotXY (specPanel, SPEC_SPEC_GRAPH, xPoint, yPoint, 1, VAL_DOUBLE, VAL_DOUBLE, VAL_SCATTER, VAL_SOLID_SQUARE, VAL_SOLID, 1, VAL_GREEN);

                harmonicCount++;
            }
        }

        SetAxisScalingMode (specPanel, SPEC_SPEC_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0.0, xRange);
        SetCtrlVal (specPanel, SPEC_PEAK_MAG, maxMagnitude);
    }
    else
    {
        for (i = 1; i < N; i++)
        {
            if (i < N / 2)
                freq = i * sampleRate / N;
            else
                freq = (i - N) * sampleRate / N;

            magnitude = sqrt(real[i] * real[i] + imag[i] * imag[i]) / N;

            if (magnitude >= threshold / 2.0 && fabs(freq) <= xRange)
            {
                xLine[0] = freq;
                xLine[1] = freq;
                yLine[0] = 0.0;
                yLine[1] = magnitude;

                PlotXY (specPanel, SPEC_SPEC_GRAPH, xLine, yLine, 2, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 4, VAL_GREEN);

                xPoint[0] = freq;
                yPoint[0] = magnitude;

                PlotXY (specPanel, SPEC_SPEC_GRAPH, xPoint, yPoint, 1, VAL_DOUBLE, VAL_DOUBLE, VAL_SCATTER, VAL_SOLID_SQUARE, VAL_SOLID, 1, VAL_GREEN);

                harmonicCount++;
            }
        }

        SetAxisScalingMode (specPanel, SPEC_SPEC_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, -xRange, xRange);
        SetCtrlVal (specPanel, SPEC_PEAK_MAG, maxMagnitude / 2.0);
    }

    SetCtrlVal (specPanel, SPEC_PEAK_FREQ, peakFrequency);
    SetCtrlVal (specPanel, SPEC_HARMONICS, (double) harmonicCount);

    if (maxMagnitude > 0.0)
    {
        if (mode == 0)
            SetAxisScalingMode (specPanel, SPEC_SPEC_GRAPH, VAL_LEFT_YAXIS, VAL_MANUAL, 0.0, maxMagnitude * 1.2);
        else
            SetAxisScalingMode (specPanel, SPEC_SPEC_GRAPH, VAL_LEFT_YAXIS, VAL_MANUAL, 0.0, (maxMagnitude / 2.0) * 1.2);
    }
}

int CVICALLBACK FilterCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    int filterType;
    int inputSource;
    int order;
    int status;
    int fftStatus1;
    int fftStatus2;
    int waveform1;
    int waveform2;
    int selectedWaveform;
    int i;

    double cutoff;
    double lowCutoff;
    double highCutoff;

    double frequency1;
    double frequency2;
    double selectedFrequency;

    double selectedSampleRate;

    double *selectedSignal;

    double timeData[N] = {0};

    double inputReal[N] = {0};
    double inputImag[N] = {0};

    double outputReal[N] = {0};
    double outputImag[N] = {0};

    double inputFreq[N / 2] = {0};
    double inputMag[N / 2] = {0};

    double outputFreq[N / 2] = {0};
    double outputMag[N / 2] = {0};

    double dt;
    double xMax;

    double maxInputMag;
    double maxOutputMag;

    double thresholdInput;
    double thresholdOutput;

    double spectrumRange;
    double nyquistFrequency;

    char filterError[256];

    double xLine[2] = {0};
    double yLine[2] = {0};

    double xPoint[1] = {0};
    double yPoint[1] = {0};

    if (event != EVENT_COMMIT)
        return 0;

    GetCtrlVal (filterPanel, FILTER_FILTER_TYPE, &filterType);

    GetCtrlVal (filterPanel, FILTER_FILTER_INPUT_S, &inputSource);

    GetCtrlVal (filterPanel, FILTER_FILTER_CUTOFF, &cutoff);

    GetCtrlVal (filterPanel, FILTER_FILTER_LOW_CUTOFF, &lowCutoff);

    GetCtrlVal (filterPanel, FILTER_FILTER_HIGH_CUTOFF, &highCutoff);

    GetCtrlVal (filterPanel, FILTER_FILTER_ORDER, &order);

    GetCtrlVal (scopePanel, SCOPE_GEN_WAVEFORM, &waveform1);

    GetCtrlVal (scopePanel, SCOPE_GEN_FREQ, &frequency1);

    GetCtrlVal (scopePanel, SCOPE_MT_GEN2_WAVEFORM, &waveform2);

    GetCtrlVal (scopePanel, SCOPE_MT_GEN2_FREQ, &frequency2);

    selectedSignal = NULL;

    selectedSampleRate = 0.0;

    selectedFrequency = 0.0;

    selectedWaveform = 0;

    if (inputSource == 0)
    {
        if (arduinoConnected)
        {
            MessagePopup ("Filter Lab", "CH1 is currently being used by the external Arduino input. Select EXTERNAL as the filter input or disconnect the Arduino.");
            return 0;
        }

        selectedSignal = signalData;

        selectedSampleRate = sampleRate;

        selectedFrequency = frequency1;

        selectedWaveform = waveform1;
    }
    else if (inputSource == 1)
    {
        selectedSignal = signal2Data;

        selectedSampleRate = sampleRate2;

        selectedFrequency = frequency2;

        selectedWaveform = waveform2;
    }
    else if (inputSource == 2)
    {
        selectedSignal = combinedSignal;

        selectedSampleRate = combinedSampleRate;

        if (frequency1 >= frequency2)
            selectedFrequency = frequency1;
        else
            selectedFrequency = frequency2;

        if (waveform1 != 0 || waveform2 != 0)
            selectedWaveform = 1;
        else
            selectedWaveform = 0;
    }
    else if (inputSource == 3)
    {
        if (!arduinoConnected)
        {
            MessagePopup ("Filter Lab", "Connect the Arduino before selecting EXTERNAL input.");
            return 0;
        }

        if (arduinoSampleCount < 8)
        {
            MessagePopup ("Filter Lab", "Not enough external samples yet. Wait a moment and apply the filter again.");
            return 0;
        }

        selectedSignal = signalData;

        selectedSampleRate = ARDUINO_SAMPLE_RATE;

        selectedFrequency = ARDUINO_SAMPLE_RATE / 2.0;

        selectedWaveform = 1;
    }

    if (selectedSignal == NULL || selectedSampleRate <= 0.0)
    {
        DeleteGraphPlot (filterPanel, FILTER_FILTER_INPUT_GRAPH, -1, VAL_IMMEDIATE_DRAW);

        DeleteGraphPlot (filterPanel, FILTER_FILTER_OUTPUT_GRAPH, -1, VAL_IMMEDIATE_DRAW);

        DeleteGraphPlot (filterPanel, FILTER_FILTER_INPUT_SPECTRUM, -1, VAL_IMMEDIATE_DRAW);

        DeleteGraphPlot (filterPanel, FILTER_FILTER_OUTPUT_SPECTRU, -1, VAL_IMMEDIATE_DRAW);

        filterInputSampleRate = 0.0;

        return 0;
    }

    if (inputSource == 3 && arduinoSampleCount < N)
    {
        for (i = 0; i < arduinoSampleCount; i++)
            filterInputData[i] = selectedSignal[i];

        for (i = arduinoSampleCount; i < N; i++)
            filterInputData[i] = 0.0;
    }
    else
    {
        for (i = 0; i < N; i++)
            filterInputData[i] = selectedSignal[i];
    }

    filterInputSampleRate = selectedSampleRate;
    filterInputSource = inputSource;

    dt = 1.0 / selectedSampleRate;

    for (i = 0; i < N; i++)
        timeData[i] = i * dt * 1000.0;

    nyquistFrequency = filterInputSampleRate / 2.0;

    if (filterType == 0 || filterType == 1)
    {
        if (cutoff <= 0.0 || cutoff >= nyquistFrequency)
        {
            sprintf (filterError, "Cutoff frequency must be greater than 0 Hz and lower than %.2f Hz for the selected input.", nyquistFrequency);
            MessagePopup ("Filter Lab", filterError);
            return 0;
        }
    }
    else
    {
        if (lowCutoff <= 0.0 || highCutoff <= lowCutoff || highCutoff >= nyquistFrequency)
        {
            sprintf (filterError, "For BPF use: 0 < Low Cutoff < High Cutoff < %.2f Hz for the selected input.", nyquistFrequency);
            MessagePopup ("Filter Lab", filterError);
            return 0;
        }
    }

    if (filterType == 0)
        status = Bw_LPF (filterInputData, N, filterInputSampleRate, cutoff, order, filteredData);
    else if (filterType == 1)
        status = Bw_HPF (filterInputData, N, filterInputSampleRate, cutoff, order, filteredData);
    else
        status = Bw_BPF (filterInputData, N, filterInputSampleRate, lowCutoff, highCutoff, order, filteredData);

    if (status < 0)
        return 0;

    DeleteGraphPlot (filterPanel, FILTER_FILTER_INPUT_GRAPH, -1, VAL_IMMEDIATE_DRAW);

    DeleteGraphPlot (filterPanel, FILTER_FILTER_OUTPUT_GRAPH, -1, VAL_IMMEDIATE_DRAW);

    PlotXY (filterPanel, FILTER_FILTER_INPUT_GRAPH, timeData, filterInputData, N, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 2, VAL_GREEN);

    PlotXY (filterPanel, FILTER_FILTER_OUTPUT_GRAPH, timeData, filteredData, N, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 2, VAL_GREEN);

    xMax = timeData[N - 1];

    SetAxisScalingMode (filterPanel, FILTER_FILTER_INPUT_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0.0, xMax);

    SetAxisScalingMode (filterPanel, FILTER_FILTER_OUTPUT_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0.0, xMax);

    for (i = 0; i < N; i++)
    {
        inputReal[i] = filterInputData[i];

        inputImag[i] = 0.0;

        outputReal[i] = filteredData[i];

        outputImag[i] = 0.0;
    }

    fftStatus1 = FFT (inputReal, inputImag, N);

    fftStatus2 = FFT (outputReal, outputImag, N);

    if (fftStatus1 < 0 || fftStatus2 < 0)
        return 0;

    maxInputMag = 0.0;

    maxOutputMag = 0.0;

    for (i = 0; i < N / 2; i++)
    {
        inputFreq[i] = i * selectedSampleRate / N;

        outputFreq[i] = i * selectedSampleRate / N;

        inputMag[i] = sqrt(inputReal[i] * inputReal[i] + inputImag[i] * inputImag[i]) * 2.0 / N;

        outputMag[i] = sqrt(outputReal[i] * outputReal[i] + outputImag[i] * outputImag[i]) * 2.0 / N;

        if (i == 0)
        {
            inputMag[i] /= 2.0;

            outputMag[i] /= 2.0;
        }

        if (inputMag[i] > maxInputMag)
            maxInputMag = inputMag[i];

        if (outputMag[i] > maxOutputMag)
            maxOutputMag = outputMag[i];
    }

    thresholdInput = maxInputMag * 0.02;

    thresholdOutput = maxOutputMag * 0.02;

    if (inputSource == 3)
        spectrumRange = selectedSampleRate / 2.0;
    else if (selectedWaveform == 0)
        spectrumRange = selectedFrequency * 2.0;
    else
        spectrumRange = selectedFrequency * 11.0;

    if (spectrumRange <= 0.0)
        spectrumRange = 1000.0;

    if (spectrumRange > selectedSampleRate / 2.0)
        spectrumRange = selectedSampleRate / 2.0;

    DeleteGraphPlot (filterPanel, FILTER_FILTER_INPUT_SPECTRUM, -1, VAL_IMMEDIATE_DRAW);

    DeleteGraphPlot (filterPanel, FILTER_FILTER_OUTPUT_SPECTRU, -1, VAL_IMMEDIATE_DRAW);

    for (i = 0; i < N / 2; i++)
    {
        if (inputMag[i] >= thresholdInput && inputFreq[i] <= spectrumRange)
        {
            xLine[0] = inputFreq[i];

            xLine[1] = inputFreq[i];

            yLine[0] = 0.0;

            yLine[1] = inputMag[i];

            PlotXY (filterPanel, FILTER_FILTER_INPUT_SPECTRUM, xLine, yLine, 2, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 4, VAL_GREEN);

            xPoint[0] = inputFreq[i];

            yPoint[0] = inputMag[i];

            PlotXY (filterPanel, FILTER_FILTER_INPUT_SPECTRUM, xPoint, yPoint, 1, VAL_DOUBLE, VAL_DOUBLE, VAL_SCATTER, VAL_SOLID_SQUARE, VAL_SOLID, 1, VAL_GREEN);
        }
    }

    for (i = 0; i < N / 2; i++)
    {
        if (outputMag[i] >= thresholdOutput && outputFreq[i] <= spectrumRange)
        {
            xLine[0] = outputFreq[i];

            xLine[1] = outputFreq[i];

            yLine[0] = 0.0;

            yLine[1] = outputMag[i];

            PlotXY (filterPanel, FILTER_FILTER_OUTPUT_SPECTRU, xLine, yLine, 2, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 4, VAL_GREEN);

            xPoint[0] = outputFreq[i];

            yPoint[0] = outputMag[i];

            PlotXY (filterPanel, FILTER_FILTER_OUTPUT_SPECTRU, xPoint, yPoint, 1, VAL_DOUBLE, VAL_DOUBLE, VAL_SCATTER, VAL_SOLID_SQUARE, VAL_SOLID, 1, VAL_GREEN);
        }
    }

    SetAxisScalingMode (filterPanel, FILTER_FILTER_INPUT_SPECTRUM, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0.0, spectrumRange);

    SetAxisScalingMode (filterPanel, FILTER_FILTER_OUTPUT_SPECTRU, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0.0, spectrumRange);

    if (maxInputMag > 0.0)
        SetAxisScalingMode (filterPanel, FILTER_FILTER_INPUT_SPECTRUM, VAL_LEFT_YAXIS, VAL_MANUAL, 0.0, maxInputMag * 1.2);

    if (maxOutputMag > 0.0)
        SetAxisScalingMode (filterPanel, FILTER_FILTER_OUTPUT_SPECTRU, VAL_LEFT_YAXIS, VAL_MANUAL, 0.0, maxOutputMag * 1.2);

    return 0;
}

int CVICALLBACK SignalGeneratorThread (void *functionData)
{
    SignalThreadData *data;

    int i;

    double t;
    double angle;
    double phaseRad;

    data = (SignalThreadData *) functionData;

    if (data == NULL)
        return -1;

    if (data->sampleRate <= 0.0)
        return -1;

    phaseRad = data->phase * PI / 180.0;

    for (i = 0; i < N; i++)
    {
        t = i / data->sampleRate;

        angle = 2.0 * PI * data->frequency * t + phaseRad;

        if (data->waveform == 0)
            data->output[i] = data->amplitude * sin(angle) + data->offset;
        else if (data->waveform == 1)
            data->output[i] = (sin(angle) >= 0.0 ? data->amplitude : -data->amplitude) + data->offset;
        else if (data->waveform == 2)
            data->output[i] = data->amplitude * (2.0 / PI) * asin(sin(angle)) + data->offset;
        else
            data->output[i] = 0.0;
    }

    return 0;
}

int CVICALLBACK GenerateMultiThreadCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    int output1;
    int output2;

    int waveform1;
    int waveform2;

    int i;

    int status1 = 0;
    int status2 = 0;

    double frequency1;
    double frequency2;

    double amplitude1;
    double amplitude2;

    double phase1;
    double phase2;

    double offset1;
    double offset2;

    double maxFrequency;

    double displayTime;
    double mtSampleRate;
    double dt;

    double timeData[N];

    double minY;
    double maxY;
    double yMargin;

    SignalThreadData data1;
    SignalThreadData data2;

    if (event != EVENT_COMMIT)
        return 0;

    GetCtrlVal (scopePanel, SCOPE_GEN_OUTPUT, &output1);

    GetCtrlVal (scopePanel, SCOPE_GEN_WAVEFORM, &waveform1);

    GetCtrlVal (scopePanel, SCOPE_GEN_FREQ, &frequency1);

    GetCtrlVal (scopePanel, SCOPE_GEN_AMP, &amplitude1);

    GetCtrlVal (scopePanel, SCOPE_GEN_PHASE, &phase1);

    GetCtrlVal (scopePanel, SCOPE_GEN_OFFSET, &offset1);

    GetCtrlVal (scopePanel, SCOPE_MT_GEN2_OUTPUT, &output2);

    GetCtrlVal (scopePanel, SCOPE_MT_GEN2_WAVEFORM, &waveform2);

    GetCtrlVal (scopePanel, SCOPE_MT_GEN2_FREQ, &frequency2);

    GetCtrlVal (scopePanel, SCOPE_MT_GEN2_AMP, &amplitude2);

    GetCtrlVal (scopePanel, SCOPE_MT_GEN2_PHASE, &phase2);

    GetCtrlVal (scopePanel, SCOPE_MT_GEN2_OFFSET, &offset2);

    if (!output1 && !output2)
    {
        combinedSampleRate = 0.0;

        for (i = 0; i < N; i++)
        {
            signal1Data[i] = 0.0;

            signal2Data[i] = 0.0;

            combinedSignal[i] = 0.0;
        }

        DeleteGraphPlot (scopePanel, SCOPE_MT_COMBINED_GRAPH, -1, VAL_IMMEDIATE_DRAW);

        return 0;
    }

    maxFrequency = 0.0;

    if (output1 && frequency1 > maxFrequency)
        maxFrequency = frequency1;

    if (output2 && frequency2 > maxFrequency)
        maxFrequency = frequency2;

    if (maxFrequency <= 0.0)
        maxFrequency = 1000.0;

    displayTime = 5.0 / maxFrequency;

    dt = displayTime / N;

    mtSampleRate = 1.0 / dt;

    combinedSampleRate = mtSampleRate;

    data1.waveform = waveform1;
    data1.frequency = frequency1;
    data1.amplitude = fabs(amplitude1);
    data1.phase = phase1;
    data1.offset = offset1;
    data1.sampleRate = mtSampleRate;
    data1.output = signal1Data;

    data2.waveform = waveform2;
    data2.frequency = frequency2;
    data2.amplitude = fabs(amplitude2);
    data2.phase = phase2;
    data2.offset = offset2;
    data2.sampleRate = mtSampleRate;
    data2.output = signal2Data;

    if (!output1)
    {
        for (i = 0; i < N; i++)
            signal1Data[i] = 0.0;
    }

    if (!output2)
    {
        for (i = 0; i < N; i++)
            signal2Data[i] = 0.0;
    }

    threadID1 = 0;
    threadID2 = 0;

    if (output1)
    {
        status1 = CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, SignalGeneratorThread, &data1, &threadID1);

        if (status1 < 0)
        {
            MessagePopup ("Multi-Thread Error", "Could not start Generator 1 thread.");

            combinedSampleRate = 0.0;

            return 0;
        }
    }

    if (output2)
    {
        status2 = CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, SignalGeneratorThread, &data2, &threadID2);

        if (status2 < 0)
        {
            if (threadID1 != 0)
            {
                CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE, threadID1, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);

                CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE, threadID1);

                threadID1 = 0;
            }

            combinedSampleRate = 0.0;

            MessagePopup ("Multi-Thread Error", "Could not start Generator 2 thread.");

            return 0;
        }
    }

    if (threadID1 != 0)
        CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE, threadID1, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);

    if (threadID2 != 0)
        CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE, threadID2, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);

    for (i = 0; i < N; i++)
    {
        combinedSignal[i] = signal1Data[i] + signal2Data[i];

        timeData[i] = i * dt * 1000.0;
    }

    if (threadID1 != 0)
    {
        CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE, threadID1);

        threadID1 = 0;
    }

    if (threadID2 != 0)
    {
        CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE, threadID2);

        threadID2 = 0;
    }

    minY = combinedSignal[0];

    maxY = combinedSignal[0];

    for (i = 1; i < N; i++)
    {
        if (combinedSignal[i] < minY)
            minY = combinedSignal[i];

        if (combinedSignal[i] > maxY)
            maxY = combinedSignal[i];
    }

    yMargin = (maxY - minY) * 0.2;

    if (yMargin < 1.0)
        yMargin = 1.0;

    DeleteGraphPlot (scopePanel, SCOPE_MT_COMBINED_GRAPH, -1, VAL_IMMEDIATE_DRAW);

    PlotXY (scopePanel, SCOPE_MT_COMBINED_GRAPH, timeData, combinedSignal, N, VAL_DOUBLE, VAL_DOUBLE, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 2, VAL_GREEN);

    SetAxisScalingMode (scopePanel, SCOPE_MT_COMBINED_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0.0, displayTime * 1000.0);

    SetAxisScalingMode (scopePanel, SCOPE_MT_COMBINED_GRAPH, VAL_LEFT_YAXIS, VAL_MANUAL, minY - yMargin, maxY + yMargin);

    return 0;
}

int CVICALLBACK SaveDataCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    int fileHandle;
    int i;
    int waveform;
    int filterType;
    int order;
    int result;

    double frequency;
    double amplitude;
    double phase;
    double offset;

    double cutoff;
    double lowCutoff;
    double highCutoff;

    double dt;
    double timeMs;

    char filePath[MAX_PATHNAME_LEN];
    char line[256];

    if (event != EVENT_COMMIT)
        return 0;

    GetCtrlVal (scopePanel, SCOPE_GEN_WAVEFORM, &waveform);

    GetCtrlVal (scopePanel, SCOPE_GEN_FREQ, &frequency);

    GetCtrlVal (scopePanel, SCOPE_GEN_AMP, &amplitude);

    GetCtrlVal (scopePanel, SCOPE_GEN_PHASE, &phase);

    GetCtrlVal (scopePanel, SCOPE_GEN_OFFSET, &offset);

    GetCtrlVal (filterPanel, FILTER_FILTER_TYPE, &filterType);

    GetCtrlVal (filterPanel, FILTER_FILTER_CUTOFF, &cutoff);

    GetCtrlVal (filterPanel, FILTER_FILTER_LOW_CUTOFF, &lowCutoff);

    GetCtrlVal (filterPanel, FILTER_FILTER_HIGH_CUTOFF, &highCutoff);

    GetCtrlVal (filterPanel, FILTER_FILTER_ORDER, &order);

    result = FileSelectPopup ("", "experiment.csv", "*.csv", "Save Experiment Data", VAL_SAVE_BUTTON, 0, 1, 1, 0, filePath);

    if (result <= 0)
        return 0;

    fileHandle = OpenFile (filePath, VAL_WRITE_ONLY, VAL_TRUNCATE, VAL_ASCII);

    if (fileHandle < 0)
    {
        MessagePopup ("Save Data", "Could not create the file.");

        return 0;
    }

    Fmt (line, "Waveform,%d", waveform);
    WriteLine (fileHandle, line, -1);

    Fmt (line, "Frequency_Hz,%f", frequency);
    WriteLine (fileHandle, line, -1);

    Fmt (line, "Amplitude_V,%f", amplitude);
    WriteLine (fileHandle, line, -1);

    Fmt (line, "Phase_deg,%f", phase);
    WriteLine (fileHandle, line, -1);

    Fmt (line, "DC_Offset_V,%f", offset);
    WriteLine (fileHandle, line, -1);

    Fmt (line, "Filter_Type,%d", filterType);
    WriteLine (fileHandle, line, -1);

    Fmt (line, "Cutoff_Hz,%f", cutoff);
    WriteLine (fileHandle, line, -1);

    Fmt (line, "Low_Cutoff_Hz,%f", lowCutoff);
    WriteLine (fileHandle, line, -1);

    Fmt (line, "High_Cutoff_Hz,%f", highCutoff);
    WriteLine (fileHandle, line, -1);

    Fmt (line, "Filter_Order,%d", order);
    WriteLine (fileHandle, line, -1);

    WriteLine (fileHandle, "", -1);

    WriteLine (fileHandle, "Time_ms,Input_V,Filtered_V", -1);

    if (filterInputSampleRate > 0.0)
        dt = 1.0 / filterInputSampleRate;
    else
        dt = 0.0;

    for (i = 0; i < N; i++)
    {
        timeMs = i * dt * 1000.0;

        Fmt (line, "%f,%f,%f", timeMs, filterInputData[i], filteredData[i]);

        WriteLine (fileHandle, line, -1);
    }

    CloseFile (fileHandle);

    MessagePopup ("Save Data", "Experiment data saved successfully.");

    return 0;
}

int CVICALLBACK ExportExcelCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    int error;
    int result;
    int i;

    int waveform;
    int filterType;
    int order;

    double frequency;
    double amplitude;
    double phase;
    double offset;

    double cutoff;
    double lowCutoff;
    double highCutoff;

    double dt;

    char filePath[MAX_PATHNAME_LEN];
    char rangeText[64];

    char waveformText[32];
    char filterText[32];

    char frequencyText[64];
    char amplitudeText[64];
    char phaseText[64];
    char offsetText[64];

    char cutoffText[64];
    char lowCutoffText[64];
    char highCutoffText[64];

    char orderText[64];
    char sampleRateText[64];

    CAObjHandle excelApp = 0;
    CAObjHandle excelWorkbooks = 0;
    CAObjHandle excelWorkbook = 0;
    CAObjHandle excelWorksheet = 0;
    CAObjHandle excelSheets = 0;
    CAObjHandle excelChart = 0;

    CAObjHandle summaryRange = 0;
    CAObjHandle titleRange = 0;
    CAObjHandle labelRange = 0;
    CAObjHandle headerRange = 0;
    CAObjHandle dataRange = 0;
    CAObjHandle allRange = 0;
    CAObjHandle chartSourceRange = 0;

    CAObjHandle titleFont = 0;
    CAObjHandle labelFont = 0;
    CAObjHandle headerFont = 0;

    VARIANT summaryRangeVariant = {0};
    VARIANT summaryDataVariant = {0};

    VARIANT titleRangeVariant = {0};
    VARIANT labelRangeVariant = {0};

    VARIANT headerRangeVariant = {0};
    VARIANT headerDataVariant = {0};

    VARIANT dataRangeVariant = {0};
    VARIANT dataVariant = {0};

    VARIANT allRangeVariant = {0};

    VARIANT chartRangeVariant = {0};
    VARIANT chartSourceVariant = {0};

    VARIANT chartTitleVariant = {0};
    VARIANT xTitleVariant = {0};
    VARIANT yTitleVariant = {0};

    VARIANT boldVariant = {0};
    VARIANT autoFitResult = {0};
    VARIANT saveVariant = {0};

    char *summaryData[10][2];
    char *headers[1][3];

    double excelData[N][3];

    if (event != EVENT_COMMIT)
        return 0;

    if (filterInputSampleRate <= 0.0)
    {
        MessagePopup ("Export to Excel", "Apply a filter to an input signal first.");

        return 0;
    }

    GetCtrlVal (scopePanel, SCOPE_GEN_WAVEFORM, &waveform);

    GetCtrlVal (scopePanel, SCOPE_GEN_FREQ, &frequency);

    GetCtrlVal (scopePanel, SCOPE_GEN_AMP, &amplitude);

    GetCtrlVal (scopePanel, SCOPE_GEN_PHASE, &phase);

    GetCtrlVal (scopePanel, SCOPE_GEN_OFFSET, &offset);

    GetCtrlVal (filterPanel, FILTER_FILTER_TYPE, &filterType);

    GetCtrlVal (filterPanel, FILTER_FILTER_CUTOFF, &cutoff);

    GetCtrlVal (filterPanel, FILTER_FILTER_LOW_CUTOFF, &lowCutoff);

    GetCtrlVal (filterPanel, FILTER_FILTER_HIGH_CUTOFF, &highCutoff);

    GetCtrlVal (filterPanel, FILTER_FILTER_ORDER, &order);

    result = FileSelectPopup ("", "experiment.xlsx", "*.xlsx", "Export Experiment to Excel", VAL_SAVE_BUTTON, 0, 1, 1, 0, filePath);

    if (result <= 0)
        return 0;

    if (waveform == 0)
        strcpy (waveformText, "Sine");
    else if (waveform == 1)
        strcpy (waveformText, "Square");
    else if (waveform == 2)
        strcpy (waveformText, "Triangle");
    else
        strcpy (waveformText, "Unknown");

    if (filterType == 0)
        strcpy (filterText, "Low Pass Filter");
    else if (filterType == 1)
        strcpy (filterText, "High Pass Filter");
    else if (filterType == 2)
        strcpy (filterText, "Band Pass Filter");
    else
        strcpy (filterText, "Unknown");

    sprintf (frequencyText, "%.2f Hz", frequency);
    sprintf (amplitudeText, "%.3f V", amplitude);
    sprintf (phaseText, "%.2f deg", phase);
    sprintf (offsetText, "%.3f V", offset);
    sprintf (cutoffText, "%.2f Hz", cutoff);
    sprintf (lowCutoffText, "%.2f Hz", lowCutoff);
    sprintf (highCutoffText, "%.2f Hz", highCutoff);
    sprintf (orderText, "%d", order);
    sprintf (sampleRateText, "%.2f Hz", filterInputSampleRate);

    summaryData[0][0] = "VIRTUAL ELECTRICAL ENGINEERING LAB";
    summaryData[0][1] = "";

    summaryData[1][0] = "Waveform";
    summaryData[1][1] = waveformText;

    summaryData[2][0] = "Frequency";
    summaryData[2][1] = frequencyText;

    summaryData[3][0] = "Amplitude";
    summaryData[3][1] = amplitudeText;

    summaryData[4][0] = "Phase";
    summaryData[4][1] = phaseText;

    summaryData[5][0] = "DC Offset";
    summaryData[5][1] = offsetText;

    summaryData[6][0] = "Filter Type";
    summaryData[6][1] = filterText;

    if (filterType == 2)
    {
        summaryData[7][0] = "Low Cutoff";
        summaryData[7][1] = lowCutoffText;

        summaryData[8][0] = "High Cutoff";
        summaryData[8][1] = highCutoffText;
    }
    else
    {
        summaryData[7][0] = "Cutoff Frequency";
        summaryData[7][1] = cutoffText;

        summaryData[8][0] = "Sample Rate";
        summaryData[8][1] = sampleRateText;
    }

    summaryData[9][0] = "Filter Order";
    summaryData[9][1] = orderText;

    headers[0][0] = "Time_ms";
    headers[0][1] = "Input_V";
    headers[0][2] = "Filtered_V";

    dt = 1.0 / filterInputSampleRate;

    for (i = 0; i < N; i++)
    {
        excelData[i][0] = i * dt * 1000.0;

        excelData[i][1] = filterInputData[i];

        excelData[i][2] = filteredData[i];
    }

    error = Excel_NewApp (NULL, 1, LOCALE_NEUTRAL, 0, &excelApp);

    if (error < 0)
        goto ExcelError;

    error = Excel_AppSetVisible (excelApp, NULL, VTRUE);

    if (error < 0)
        goto ExcelError;

    error = Excel_AppGetWorkbooks (excelApp, NULL, &excelWorkbooks);

    if (error < 0)
        goto ExcelError;

    error = Excel_WorkbooksAdd (excelWorkbooks, NULL, CA_DEFAULT_VAL, &excelWorkbook);

    if (error < 0)
        goto ExcelError;

    error = Excel_AppGetActiveSheet (excelApp, NULL, &excelWorksheet);

    if (error < 0)
        goto ExcelError;

    error = Excel_WorksheetActivate (excelWorksheet, NULL);

    if (error < 0)
        goto ExcelError;

    Excel_WorksheetSetName (excelWorksheet, NULL, "Experiment Data");

    CA_VariantSetCString (&summaryRangeVariant, "A1:B10");

    error = Excel_WorksheetGetRange (excelWorksheet, NULL, summaryRangeVariant, CA_DEFAULT_VAL, &summaryRange);

    if (error < 0)
        goto ExcelError;

    error = CA_VariantSet2DArray (&summaryDataVariant, CAVT_CSTRING, 10, 2, summaryData);

    if (error < 0)
        goto ExcelError;

    error = Excel_RangeSetValue2 (summaryRange, NULL, summaryDataVariant);

    if (error < 0)
        goto ExcelError;

    CA_VariantSetCString (&headerRangeVariant, "A12:C12");

    error = Excel_WorksheetGetRange (excelWorksheet, NULL, headerRangeVariant, CA_DEFAULT_VAL, &headerRange);

    if (error < 0)
        goto ExcelError;

    error = CA_VariantSet2DArray (&headerDataVariant, CAVT_CSTRING, 1, 3, headers);

    if (error < 0)
        goto ExcelError;

    error = Excel_RangeSetValue2 (headerRange, NULL, headerDataVariant);

    if (error < 0)
        goto ExcelError;

    sprintf (rangeText, "A13:C%d", N + 12);

    CA_VariantSetCString (&dataRangeVariant, rangeText);

    error = Excel_WorksheetGetRange (excelWorksheet, NULL, dataRangeVariant, CA_DEFAULT_VAL, &dataRange);

    if (error < 0)
        goto ExcelError;

    error = CA_VariantSet2DArray (&dataVariant, CAVT_DOUBLE, N, 3, excelData);

    if (error < 0)
        goto ExcelError;

    error = Excel_RangeSetValue2 (dataRange, NULL, dataVariant);

    if (error < 0)
        goto ExcelError;

    sprintf (rangeText, "A12:C%d", N + 12);

    CA_VariantSetCString (&chartRangeVariant, rangeText);

    error = Excel_WorksheetGetRange (excelWorksheet, NULL, chartRangeVariant, CA_DEFAULT_VAL, &chartSourceRange);

    if (error < 0)
        goto ExcelError;

    error = CA_VariantSetObjHandle (&chartSourceVariant, chartSourceRange, CAVT_DISPATCH);

    if (error < 0)
        goto ExcelError;

    error = Excel_AppGetSheets (excelApp, NULL, &excelSheets);

    if (error < 0)
        goto ExcelError;

    error = Excel_SheetsAdd (excelSheets, NULL, CA_DEFAULT_VAL, CA_DEFAULT_VAL, CA_DEFAULT_VAL, CA_VariantLong (ExcelConst_xlChart), NULL);

    if (error < 0)
        goto ExcelError;

    error = Excel_AppGetActiveChart (excelApp, NULL, &excelChart);

    if (error < 0)
        goto ExcelError;

    CA_VariantSetCString (&chartTitleVariant, "Input Signal vs Filtered Signal");

    CA_VariantSetCString (&xTitleVariant, "Time [ms]");

    CA_VariantSetCString (&yTitleVariant, "Amplitude [V]");

    error = Excel_ChartChartWizard (excelChart, NULL, chartSourceVariant, CA_VariantLong (ExcelConst_xlXYScatter), CA_VariantInt (2), CA_VariantLong (ExcelConst_xlColumns), CA_VariantInt (1), CA_VariantInt (1), CA_VariantBool (VTRUE), chartTitleVariant, xTitleVariant, yTitleVariant, CA_DEFAULT_VAL);

    if (error < 0)
        goto ExcelError;

    CA_VariantSetBool (&boldVariant, VTRUE);

    CA_VariantSetCString (&titleRangeVariant, "A1:B1");

    if (Excel_WorksheetGetRange (excelWorksheet, NULL, titleRangeVariant, CA_DEFAULT_VAL, &titleRange) >= 0)
    {
        if (Excel_RangeGetFont (titleRange, NULL, &titleFont) >= 0)
            Excel_FontSetBold (titleFont, NULL, boldVariant);
    }

    CA_VariantSetCString (&labelRangeVariant, "A2:A10");

    if (Excel_WorksheetGetRange (excelWorksheet, NULL, labelRangeVariant, CA_DEFAULT_VAL, &labelRange) >= 0)
    {
        if (Excel_RangeGetFont (labelRange, NULL, &labelFont) >= 0)
            Excel_FontSetBold (labelFont, NULL, boldVariant);
    }

    if (Excel_RangeGetFont (headerRange, NULL, &headerFont) >= 0)
        Excel_FontSetBold (headerFont, NULL, boldVariant);

    CA_VariantSetCString (&allRangeVariant, "A:C");

    if (Excel_WorksheetGetRange (excelWorksheet, NULL, allRangeVariant, CA_DEFAULT_VAL, &allRange) >= 0)
        Excel_RangeAutoFit (allRange, NULL, &autoFitResult);

    CA_VariantSetCString (&saveVariant, filePath);

    error = Excel_WorkbookSaveAs (excelWorkbook, NULL, saveVariant, CA_DEFAULT_VAL, CA_DEFAULT_VAL, CA_DEFAULT_VAL, CA_DEFAULT_VAL, CA_DEFAULT_VAL, ExcelConst_xlNoChange, CA_DEFAULT_VAL, CA_DEFAULT_VAL, CA_DEFAULT_VAL, CA_DEFAULT_VAL, CA_DEFAULT_VAL, CA_DEFAULT_VAL);

    if (error < 0)
        goto ExcelError;

    CA_VariantClear (&summaryRangeVariant);
    CA_VariantClear (&summaryDataVariant);
    CA_VariantClear (&titleRangeVariant);
    CA_VariantClear (&labelRangeVariant);
    CA_VariantClear (&headerRangeVariant);
    CA_VariantClear (&headerDataVariant);
    CA_VariantClear (&dataRangeVariant);
    CA_VariantClear (&dataVariant);
    CA_VariantClear (&allRangeVariant);
    CA_VariantClear (&chartRangeVariant);
    CA_VariantClear (&chartSourceVariant);
    CA_VariantClear (&chartTitleVariant);
    CA_VariantClear (&xTitleVariant);
    CA_VariantClear (&yTitleVariant);
    CA_VariantClear (&boldVariant);
    CA_VariantClear (&autoFitResult);
    CA_VariantClear (&saveVariant);

    if (titleFont)
        CA_DiscardObjHandle (titleFont);

    if (labelFont)
        CA_DiscardObjHandle (labelFont);

    if (headerFont)
        CA_DiscardObjHandle (headerFont);

    if (summaryRange)
        CA_DiscardObjHandle (summaryRange);

    if (titleRange)
        CA_DiscardObjHandle (titleRange);

    if (labelRange)
        CA_DiscardObjHandle (labelRange);

    if (headerRange)
        CA_DiscardObjHandle (headerRange);

    if (dataRange)
        CA_DiscardObjHandle (dataRange);

    if (allRange)
        CA_DiscardObjHandle (allRange);

    if (chartSourceRange)
        CA_DiscardObjHandle (chartSourceRange);

    if (excelChart)
        CA_DiscardObjHandle (excelChart);

    if (excelSheets)
        CA_DiscardObjHandle (excelSheets);

    if (excelWorksheet)
        CA_DiscardObjHandle (excelWorksheet);

    if (excelWorkbook)
        CA_DiscardObjHandle (excelWorkbook);

    if (excelWorkbooks)
        CA_DiscardObjHandle (excelWorkbooks);

    if (excelApp)
        CA_DiscardObjHandle (excelApp);

    MessagePopup ("Export to Excel", "Excel report and chart created successfully.");

    return 0;

ExcelError:

    CA_VariantClear (&summaryRangeVariant);
    CA_VariantClear (&summaryDataVariant);
    CA_VariantClear (&titleRangeVariant);
    CA_VariantClear (&labelRangeVariant);
    CA_VariantClear (&headerRangeVariant);
    CA_VariantClear (&headerDataVariant);
    CA_VariantClear (&dataRangeVariant);
    CA_VariantClear (&dataVariant);
    CA_VariantClear (&allRangeVariant);
    CA_VariantClear (&chartRangeVariant);
    CA_VariantClear (&chartSourceVariant);
    CA_VariantClear (&chartTitleVariant);
    CA_VariantClear (&xTitleVariant);
    CA_VariantClear (&yTitleVariant);
    CA_VariantClear (&boldVariant);
    CA_VariantClear (&autoFitResult);
    CA_VariantClear (&saveVariant);

    if (titleFont)
        CA_DiscardObjHandle (titleFont);

    if (labelFont)
        CA_DiscardObjHandle (labelFont);

    if (headerFont)
        CA_DiscardObjHandle (headerFont);

    if (summaryRange)
        CA_DiscardObjHandle (summaryRange);

    if (titleRange)
        CA_DiscardObjHandle (titleRange);

    if (labelRange)
        CA_DiscardObjHandle (labelRange);

    if (headerRange)
        CA_DiscardObjHandle (headerRange);

    if (dataRange)
        CA_DiscardObjHandle (dataRange);

    if (allRange)
        CA_DiscardObjHandle (allRange);

    if (chartSourceRange)
        CA_DiscardObjHandle (chartSourceRange);

    if (excelChart)
        CA_DiscardObjHandle (excelChart);

    if (excelSheets)
        CA_DiscardObjHandle (excelSheets);

    if (excelWorksheet)
        CA_DiscardObjHandle (excelWorksheet);

    if (excelWorkbook)
        CA_DiscardObjHandle (excelWorkbook);

    if (excelWorkbooks)
        CA_DiscardObjHandle (excelWorkbooks);

    if (excelApp)
        CA_DiscardObjHandle (excelApp);

    MessagePopup ("Export to Excel", "ActiveX Excel export failed.");

    return 0;
}


int CVICALLBACK ExternalConnectCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    int status;
    int comPortValue;

    if (event != EVENT_COMMIT)
        return 0;

    if (!arduinoConnected)
    {
        GetCtrlVal (scopePanel, SCOPE_SCOPE_EXT_COM_PORT, &comPortValue);
        arduinoComPort = comPortValue;

        if (arduinoComPort <= 0)
        {
            MessagePopup ("Arduino", "Enter a valid COM port number.");
            return 0;
        }

        status = OpenComConfig (arduinoComPort, "", 115200, 0, 8, 1, 512, 512);

        if (status < 0)
        {
            arduinoComPort = 0;
            MessagePopup ("Arduino", "Could not connect to Arduino. Close the Arduino Serial Monitor and check the COM port.");
            return 0;
        }

        FlushInQ (arduinoComPort);
        FlushOutQ (arduinoComPort);

        arduinoLineLength = 0;
        arduinoLineBuffer[0] = '\0';
        arduinoSampleCount = 0;
        sampleRate = ARDUINO_SAMPLE_RATE;

        for (status = 0; status < N; status++)
            signalData[status] = 0.0;

        arduinoConnected = 1;
        SetCtrlVal (scopePanel, SCOPE_SCOPE_EXT_VOLTAGE, 0.0);

        MessagePopup ("Arduino", "Arduino connected successfully.");
    }
    else
    {
        CloseCom (arduinoComPort);
        arduinoConnected = 0;
        arduinoComPort = 0;
        arduinoLineLength = 0;
        arduinoLineBuffer[0] = '\0';
        arduinoSampleCount = 0;
        sampleRate = 0.0;
        SetCtrlVal (scopePanel, SCOPE_SCOPE_EXT_VOLTAGE, 0.0);

        MessagePopup ("Arduino", "Arduino disconnected.");
    }

    return 0;
}

int CVICALLBACK ResetAllCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    int i;
    double currentTime;

    if (event != EVENT_COMMIT)
        return 0;

    if (arduinoConnected)
    {
        CloseCom (arduinoComPort);
        arduinoConnected = 0;
        arduinoComPort = 0;
    }

    arduinoLineLength = 0;
    arduinoLineBuffer[0] = '\0';
    arduinoSampleCount = 0;

    SetCtrlVal (scopePanel, SCOPE_SCOPE_EXT_VOLTAGE, 0.0);

    SetCtrlVal (scopePanel, SCOPE_GEN_OUTPUT, 0);
    SetCtrlVal (scopePanel, SCOPE_GEN_WAVEFORM, 0);
    SetCtrlVal (scopePanel, SCOPE_GEN_FREQ, 1000.0);
    SetCtrlVal (scopePanel, SCOPE_GEN_AMP, 1.0);
    SetCtrlVal (scopePanel, SCOPE_GEN_PHASE, 0.0);
    SetCtrlVal (scopePanel, SCOPE_GEN_OFFSET, 0.0);

    SetCtrlVal (scopePanel, SCOPE_MT_GEN2_OUTPUT, 0);
    SetCtrlVal (scopePanel, SCOPE_MT_GEN2_WAVEFORM, 0);
    SetCtrlVal (scopePanel, SCOPE_MT_GEN2_FREQ, 1000.0);
    SetCtrlVal (scopePanel, SCOPE_MT_GEN2_AMP, 1.0);
    SetCtrlVal (scopePanel, SCOPE_MT_GEN2_PHASE, 0.0);
    SetCtrlVal (scopePanel, SCOPE_MT_GEN2_OFFSET, 0.0);

    SetCtrlVal (scopePanel, SCOPE_MEAS_VPP, 0.0);
    SetCtrlVal (scopePanel, SCOPE_MEAS_RMS, 0.0);
    SetCtrlVal (scopePanel, SCOPE_MEAS_FREQ, 0.0);
    SetCtrlVal (scopePanel, SCOPE_MEAS_PERIOD, 0.0);

    SetCtrlVal (scopePanel, SCOPE_MT_GEN1_LED, 0);
    SetCtrlVal (scopePanel, SCOPE_MT_GEN2_LED, 0);

    thread1TotalTime = 0.0;
    thread2TotalTime = 0.0;

    currentTime = Timer();

    thread1LastTime = currentTime;
    thread2LastTime = currentTime;

    previousOutput1 = 0;
    previousOutput2 = 0;

    ledBlinkState = 0;
    lastLedBlinkTime = currentTime;

    SetCtrlVal (scopePanel, SCOPE_SCOPE_MT_THREAD1_TIME, 0.0);
    SetCtrlVal (scopePanel, SCOPE_SCOPE_MT_THREAD2_TIME, 0.0);

    sampleRate = 0.0;
    sampleRate2 = 0.0;
    combinedSampleRate = 0.0;
    filterInputSampleRate = 0.0;
    filterInputSource = 0;

    threadID1 = 0;
    threadID2 = 0;

    for (i = 0; i < N; i++)
    {
        signalData[i] = 0.0;
        filteredData[i] = 0.0;
        filterInputData[i] = 0.0;
        signal1Data[i] = 0.0;
        signal2Data[i] = 0.0;
        combinedSignal[i] = 0.0;
    }

    DeleteGraphPlot (scopePanel, SCOPE_SCOPE_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    DeleteGraphPlot (scopePanel, SCOPE_MT_GEN2_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    DeleteGraphPlot (scopePanel, SCOPE_MT_COMBINED_GRAPH, -1, VAL_IMMEDIATE_DRAW);

    DeleteGraphPlot (specPanel, SPEC_SPEC_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    DeleteGraphPlot (specPanel, SPEC_SPEC_BODE_MAG_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    DeleteGraphPlot (specPanel, SPEC_SPEC_BODE_PHASE_GRAPH, -1, VAL_IMMEDIATE_DRAW);

    SetCtrlVal (specPanel, SPEC_PEAK_FREQ, 0.0);
    SetCtrlVal (specPanel, SPEC_PEAK_MAG, 0.0);
    SetCtrlVal (specPanel, SPEC_HARMONICS, 0.0);

    DeleteGraphPlot (filterPanel, FILTER_FILTER_INPUT_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    DeleteGraphPlot (filterPanel, FILTER_FILTER_OUTPUT_GRAPH, -1, VAL_IMMEDIATE_DRAW);
    DeleteGraphPlot (filterPanel, FILTER_FILTER_INPUT_SPECTRUM, -1, VAL_IMMEDIATE_DRAW);
    DeleteGraphPlot (filterPanel, FILTER_FILTER_OUTPUT_SPECTRU, -1, VAL_IMMEDIATE_DRAW);

    SetCtrlVal (filterPanel, FILTER_FILTER_INPUT_S, 0);

    return 0;
}

int CVICALLBACK OutputCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    if (event == EVENT_COMMIT)
        UpdateSignal();

    return 0;
}

int CVICALLBACK TimerCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    int output1 = 0;
    int output2 = 0;
    int bytesAvailable;
    int bytesRead;
    int bytesToRead;
    int i;

    double currentTime;
    double arduinoVoltage;

    char serialChunk[256];
    char receivedChar;

    if (event == EVENT_TIMER_TICK)
    {
        if (arduinoConnected)
        {
            while ((bytesAvailable = GetInQLen (arduinoComPort)) > 0)
            {
                bytesToRead = bytesAvailable;

                if (bytesToRead > 256)
                    bytesToRead = 256;

                bytesRead = ComRd (arduinoComPort, serialChunk, bytesToRead);

                if (bytesRead <= 0)
                    break;

                for (i = 0; i < bytesRead; i++)
                {
                    receivedChar = serialChunk[i];

                    if (receivedChar == '\n')
                    {
                        if (arduinoLineLength > 0)
                        {
                            arduinoLineBuffer[arduinoLineLength] = '\0';

                            if (Scan (arduinoLineBuffer, "%f", &arduinoVoltage) > 0)
                            {
                                SetCtrlVal (scopePanel, SCOPE_SCOPE_EXT_VOLTAGE, arduinoVoltage);
                                AddArduinoSample (arduinoVoltage);
                            }

                            arduinoLineLength = 0;
                        }
                    }
                    else if (receivedChar != '\r')
                    {
                        if (arduinoLineLength < 63)
                        {
                            arduinoLineBuffer[arduinoLineLength] = receivedChar;
                            arduinoLineLength++;
                        }
                        else
                        {
                            arduinoLineLength = 0;
                        }
                    }
                }
            }
        }

        if (arduinoConnected)
            UpdateArduinoScope();
        else
            UpdateSignal();

        UpdateSignal2();

        GetCtrlVal (scopePanel, SCOPE_GEN_OUTPUT, &output1);

        GetCtrlVal (scopePanel, SCOPE_MT_GEN2_OUTPUT, &output2);

        currentTime = Timer();

        if (output1)
        {
            if (!previousOutput1)
            {
                thread1LastTime = currentTime;
                previousOutput1 = 1;
            }

            thread1TotalTime += currentTime - thread1LastTime;

            thread1LastTime = currentTime;
        }
        else
        {
            previousOutput1 = 0;

            thread1LastTime = currentTime;
        }

        if (output2)
        {
            if (!previousOutput2)
            {
                thread2LastTime = currentTime;
                previousOutput2 = 1;
            }

            thread2TotalTime += currentTime - thread2LastTime;

            thread2LastTime = currentTime;
        }
        else
        {
            previousOutput2 = 0;

            thread2LastTime = currentTime;
        }

        SetCtrlVal (scopePanel, SCOPE_SCOPE_MT_THREAD1_TIME, thread1TotalTime);

        SetCtrlVal (scopePanel, SCOPE_SCOPE_MT_THREAD2_TIME, thread2TotalTime);

        if (currentTime - lastLedBlinkTime >= 0.5)
        {
            ledBlinkState = !ledBlinkState;

            lastLedBlinkTime = currentTime;
        }

        if (output1)
            SetCtrlVal (scopePanel, SCOPE_MT_GEN1_LED, ledBlinkState);
        else
            SetCtrlVal (scopePanel, SCOPE_MT_GEN1_LED, 0);

        if (output2)
            SetCtrlVal (scopePanel, SCOPE_MT_GEN2_LED, ledBlinkState);
        else
            SetCtrlVal (scopePanel, SCOPE_MT_GEN2_LED, 0);
    }

    return 0;
}

int CVICALLBACK QUIT (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
    if (event == EVENT_COMMIT)
    {
        if (arduinoConnected)
        {
            CloseCom (arduinoComPort);
            arduinoConnected = 0;
            arduinoComPort = 0;
        }

        QuitUserInterface (0);
    }

    return 0;
}
