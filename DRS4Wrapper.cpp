#include <chrono>
#include <vector>
#include <iostream>
#include <boost/python.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#include "drs/DRS.h"

// Used for number of data points to include in channel to channel delay measurements
#define N_FIT 5

using namespace std;


class chframe {
public:
    vector<float> time;
    vector<float> vals;

    // Hack to make compilation work, not sure why :/
    friend bool operator== (chframe& p1, const chframe& p2) {
        return p1.time == p2.time && p1.vals == p2.vals;
    };
};


class frame {
public:
    long long timestamp;
    vector<chframe> channels;
};


long long getNow() {
    using namespace std::chrono;

    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}


class DRS4Wrapper {

    boost::circular_buffer<frame> framebuffer;

    DRS *drs;
    DRSBoard *b;

public:
    DRS4Wrapper() {
        framebuffer = boost::circular_buffer<frame>(1);

        drs = new DRS();
    }


    ~DRS4Wrapper() {
        delete drs;
    }


    int initBoard() {
        if (drs->GetNumberOfBoards() == 0)
            return 0;

        float sampling_speed = 5.0;
        int trigger_channel = 1;

        b = drs->GetBoard(0);

        b->Init();

        b->SetFrequency(sampling_speed, true);

        if (b->GetTransport() == 3) {
            b->SetChannelConfig(0, 8, 8);
        } else {
            b->SetChannelConfig(7, 8, 8);
        }

        b->SetDecimation(0);
        b->SetDominoMode(1);
        b->SetReadoutMode(1);
        b->SetDominoActive(1);

        if (trigger_channel == 0){
            b->EnableTrigger(1, 0);
        } else {
            b->SetTranspMode(1);
            b->EnableTrigger(1, 0);
            b->SetTriggerSource(1 << (trigger_channel-1));
        }

        // Recorded frame is 25ns before trigger.
        b->SetTriggerDelayNs(150);

        b->SetRefclk(0);
        b->SetFrequency(sampling_speed, true);
        b->EnableAcal(0, 0);
        b->EnableTcal(0, 0);

        b->StartDomino();

        return 1;
    }


    void SetFrequency(float x)      { b->SetFrequency(x, true); }
    void SetTriggerChannel(int x)   { b->SetTriggerSource(1 << (x-1)); }
    void SetTriggerPolarity(bool x) { b->SetTriggerPolarity(1 << (x-1)); }
    void SetTriggerLevel(float x) { b->SetTriggerLevel(x); }

    float GetFrequency() { return b->GetTrueFrequency(); }


    int record() {
        b->StartDomino();

        // while(b->IsBusy()) {
        //     continue;
        // }

        b->TransferWaves(9);

        frame tempf;
        tempf.timestamp = getNow();

        float tbuf[1024];
        float vbuf[1024];

        for (int i=0; i < 4; i++) {
            b->GetTime(0, (i)*2, b->GetTriggerCell(0), tbuf);
            b->GetWave(0, (i)*2, vbuf);

            chframe tempchf;
            tempchf.time = vector<float>(begin(tbuf), end(tbuf));
            tempchf.vals = vector<float>(begin(vbuf), end(vbuf));

            tempf.channels.push_back(tempchf);
        }

        framebuffer.push_back(tempf);

        return 1;
    }


    frame record2() {
        if (record() == 1)
            return framebuffer.back();
        else
            return frame();
    }


    frame getLastFrame() { return framebuffer.back(); }

    long long getTime() { return framebuffer.back().timestamp; }


    float measureLevel(int c=1) {
        vector<float> y = framebuffer.back().channels[c-1].vals;

        float l = 0;

        for (auto i : y)
            l += i;

        return l/1024;
    }


    float measurePeak(int c=1) {
        vector<float> y = framebuffer.back().channels[c-1].vals;

        float max = y[0];

        for (auto i : y)
            if (i > max)
                max = i;

        return max;
    }


    float measureNPeak(int c=1) {
        vector<float> y = framebuffer.back().channels[c-1].vals;

        float min = y[0];

        for (auto i : y)
            if (i < min)
                min = i;

        return min;
    }


    float measurePeakPeak(int c=1) {
        vector<float> x = framebuffer.back().channels[c-1].time;
        vector<float> y = framebuffer.back().channels[c-1].vals;

        int n = 1024;

        float min_x, min_y, max_x, max_y;

        min_x = max_x = x[0];
        min_y = max_y = y[0];
        for (int i=0 ; i<n ; i++) {
            if (y[i] < min_y) {
                min_x = x[i];
                min_y = y[i];
            }
            if (y[i] > max_y) {
                max_x = x[i];
                max_y = y[i];
            }
        }

        return max_y-min_y;
    }


    float measureRMS(int c=1) {
        vector<float> x = framebuffer.back().channels[c-1].time;
        vector<float> y = framebuffer.back().channels[c-1].vals;

        int n = 1024;

        float mean = 0;
        float rms  = 0;

        for (int i=0 ; i<n ; i++)
            mean += y[i];

        mean /= n;

        for (int i=0 ; i<n ; i++)
            rms += (y[i]-mean)*(y[i]-mean);

        rms = sqrt(rms/n);

        return rms;
    }


    float measureCharge(int c=1, int start=0, int end=1024) {
        vector<float> x = framebuffer.back().channels[c-1].time;
        vector<float> y = framebuffer.back().channels[c-1].vals;

        int n = 1024;

        float q = 0;
        float x1, x2, y1, y2;

        for (int i=0 ; i<n ; i++) {
            if (x[i+1] >= start && x[i] <= end) {
                if (x[i] < start) {
                    x1 = start;
                    y1 = y[i] + (y[i+1]-y[i]) * (start - x[i]) / (x[i+1] - x[i]);
                } else {
                    x1 = x[i];
                    y1 = y[i];
                }
                if (x[i+1] > end) {
                    x2 = end;
                    y2 = y[i] + (y[i+1]-y[i]) * (end - x[i]) / (x[i+1] - x[i]);
                } else {
                    x2 = x[i+1];
                    y2 = y[i+1];
                }

                q += 0.5 * (y1 + y2) * (x2 - x1);
            }
        }

        return q / 50; // signal into 50 Ohm
    }


    float measureFrequency(int channel=1) {
       float p = measurePeriod(channel);

       if (p == -1 || p == 0)
          return -1;

       return 1000/p;
    }


    float measurePeriod(int c=1) {
        vector<float> x = framebuffer.back().channels[c-1].time;
        vector<float> y = framebuffer.back().channels[c-1].vals;

        int n = 1024;

        int i, pos_edge, n_pos, n_neg;
        float miny, maxy, mean, t1, t2;

        miny = maxy = y[0];
        mean = 0;
        for (i=0 ; i<n ; i++) {
            if (y[i] > maxy)
                 maxy = y[i];
            if (y[i] < miny)
                 miny = y[i];
            mean += y[i];
        }

        if (n < 5 || maxy - miny < 10)
            return -1;

        mean = mean/n;

        /* count zero crossings */
        n_pos = n_neg = 0;
        for (i=1 ; i<n ; i++) {
            if (y[i] > mean && y[i-1] <= mean)
                n_pos++;
            if (y[i] < mean && y[i-1] >= mean)
                n_neg++;
        }

        /* search for zero crossing */
        for (i=n/2+2 ; i>1 ; i--) {
            if (n_pos > 1 && y[i] > mean && y[i-1] <= mean)
                break;
            if (n_neg > 1 && y[i] < mean && y[i-1] >= mean)
                break;
        }
        if (i == 1)
            for (i=n/2 ; i<n ; i++) {
                if (n_pos > 1 && y[i] > mean && y[i-1] <= mean)
                    break;
                if (n_neg > 1 && y[i] < mean && y[i-1] >= mean)
                    break;
          }
        if (i == n)
            return -1;

        pos_edge = y[i] > mean;

        t1 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        /* search next zero crossing */
        for (i++ ; i<n ; i++) {
            if (pos_edge && y[i] > mean && y[i-1] <= mean)
                break;
            if (!pos_edge && y[i] < mean && y[i-1] >= mean)
                break;
        }

        if (i == n)
            return -1;

        t2 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        return t2 - t1;
    }


    float measureRise(int c=1) {
        vector<float> x = framebuffer.back().channels[c-1].time;
        vector<float> y = framebuffer.back().channels[c-1].vals;

        int n = 1024;

        int i;
        float miny, maxy, t1, t2, y10, y90;

        miny = maxy = y[0];
        for (i=0 ; i<n ; i++) {
            if (y[i] > maxy)
                maxy = y[i];
            if (y[i] < miny)
                miny = y[i];
        }

        if (maxy - miny < 10)
            return -1;

        y10 = miny+0.1*(maxy-miny);
        y90 = miny+0.9*(maxy-miny);

        /* search for 10% level crossing */
        for (i=n/2+2 ; i>1 ; i--)
            if (y[i] > y10 && y[i-1] <= y10)
                break;

        if (i == 1)
            for (i=n/2 ; i<n ; i++) {
                if (y[i] > y10 && y[i-1] <= y10)
                break;
            }

        if (i == n)
            return -1;

        t1 = (y10*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        /* search for 90% level crossing */
        for (i++ ; i<n ; i++) 
            if (y[i] > y90 && y[i-1] <= y90)
                break;

        if (i == n)
            return -1;

        t2 = (y90*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        return t2 - t1;
    }


    float measureFall(int c=1) {
        vector<float> x = framebuffer.back().channels[c-1].time;
        vector<float> y = framebuffer.back().channels[c-1].vals;

        int n = 1024;

        int i;
        float miny, maxy, t1, t2, y10, y90;

        miny = maxy = y[0];
        for (i=0 ; i<n ; i++) {
            if (y[i] > maxy)
                maxy = y[i];
            if (y[i] < miny)
                miny = y[i];
        }

        if (maxy - miny < 10)
            return -1;

        y10 = miny+0.1*(maxy-miny);
        y90 = miny+0.9*(maxy-miny);

        /* search for 90% level crossing */
        for (i=n/2+2 ; i>1 ; i--)
            if (y[i] < y90 && y[i-1] >= y90)
                break;

        if (i == 1)
            for (i=n/2 ; i<n ; i++) {
                if (y[i] < y90 && y[i-1] >= y90)
                break;
            }

        if (i == n)
            return -1;

        t1 = (y90*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        /* search for 10% level crossing */
        for (i++ ; i<n ; i++) 
            if (y[i] < y10 && y[i-1] >= y10)
                break;

        if (i == n)
            return -1;

        t2 = (y10*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        return t2 - t1;
    }


    double measurePosWidth(int c=1) {
        vector<float> x = framebuffer.back().channels[c-1].time;
        vector<float> y = framebuffer.back().channels[c-1].vals;

        int n = 1024;

        int i;
        double miny, maxy, mean, t1, t2;

        miny = maxy = y[0];
        for (i=0 ; i<n ; i++) {
            if (y[i] > maxy)
                maxy = y[i];
            if (y[i] < miny)
                miny = y[i];
        }
        mean = (miny + maxy)/2;

        if (maxy - miny < 10)
            return -1;

        /* search for first pos zero crossing */
        for (i=1 ; i<n ; i++)
            if (y[i] > mean && y[i-1] <= mean)
                break;
        
        if (i == n)
            return -1;

        t1 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        /* search next neg zero crossing */
        for (i++ ; i<n ; i++)
            if (y[i] < mean && y[i-1] >= mean)
                break;
        
        if (i == n)
            return -1;

        t2 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        return t2 - t1;
    }

    double measureNegWidth(int c=1) {
        vector<float> x = framebuffer.back().channels[c-1].time;
        vector<float> y = framebuffer.back().channels[c-1].vals;

        int n = 1024;

        int i;
        double miny, maxy, mean, t1, t2;

        miny = maxy = y[0];
        for (i=0 ; i<n ; i++) {
            if (y[i] > maxy)
                maxy = y[i];
            if (y[i] < miny)
                miny = y[i];
        }

        mean = (miny + maxy)/2;

        if (maxy - miny < 10)
            return -1;

        /* search for first neg zero crossing */
        for (i=1 ; i<n ; i++)
            if (y[i] < mean && y[i-1] >= mean)
                break;
        
        if (i == n)
            return -1;

        t1 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        /* search next pos zero crossing */
        for (i++ ; i<n ; i++)
            if (y[i] > mean && y[i-1] <= mean)
                break;
        
        if (i == n)
            return -1;

        t2 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

        return t2 - t1;
    }


    void linfit(float *x, float *y, int n, float &a, float &b) {
        int i;
        float sx, sxx, sy, syy, sxy;

        sx = sxx = sy = syy = sxy = 0;

        for (i=0 ; i<n ; i++) {
            sx += x[i];
            sy += y[i];
            sxy += x[i]*y[i];
            sxx += x[i]*x[i];
            syy += y[i]*y[i];
        }

        b = (sxy - sx*sy/n) / (sxx - sx*sx/n);
        a = sy/n-b*sx/n;
          
        return;
    }


    float measureDelay(int c1=1, int c2=2, int pol1=1, int pol2=2, float thr1=100, float thr2=100) {
        vector<float> x1 = framebuffer.back().channels[c1-1].time;
        vector<float> y1 = framebuffer.back().channels[c1-1].vals;
        vector<float> x2 = framebuffer.back().channels[c2-1].time;
        vector<float> y2 = framebuffer.back().channels[c2-1].vals;
        int n = 1024;

        int i, i1l, i1r, i2l, i2r;
        float t1, t2, a, b;

        // thr = 0.1 * 1000;
        // pol = 1;
        for (i=1 ; i<n ; i++) {
            if (pol1 == 1 && y1[i] < thr1 && y1[i-1] >= thr1)
                break;
            if (pol1 == 0 && y1[i] > thr1 && y1[i-1] <= thr1)
                break;
        }
        if (i == n)
            return -1;

        t1 = (thr1*(x1[i]-x1[i-1])+x1[i-1]*y1[i]-x1[i]*y1[i-1])/(y1[i]-y1[i-1]);

        if (N_FIT > 0 && i>=N_FIT/2) {
            i1l = i-N_FIT/2;
            i1r = i1l+N_FIT-1;
            linfit(&x1[i-N_FIT/2], &y1[i-N_FIT/2], N_FIT, a, b);
            if (b != 0)
                t1 = (thr1-a)/b;
        }

        for (i=1 ; i<n ; i++) {
            if (pol2 == 1 && y2[i] < thr2 && y2[i-1] >= thr2)
                break;
            if (pol2 == 0 && y2[i] > thr2 && y2[i-1] <= thr2)
                break;
        }
        if (i == n)
            return -1;

        t2 = (thr2*(x2[i]-x2[i-1])+x2[i-1]*y2[i]-x2[i]*y2[i-1])/(y2[i]-y2[i-1]);

        if (N_FIT > 0 && i>=N_FIT/2) {
            i2l = i-N_FIT/2;
            i2r = i2l+N_FIT-1;
            linfit(&x2[i-N_FIT/2], &y2[i-N_FIT/2], N_FIT, a, b);
            if (b != 0)
                t2 = (thr2-a)/b;
        }

        return t2 - t1;
    }
};


BOOST_PYTHON_MODULE(DRS4Wrapper) {
    using namespace boost::python;

    class_<frame>("Frame")
        .def_readwrite("timestamp", &frame::timestamp)
        .def_readwrite("channels", &frame::channels)
    ;

    class_<chframe>("ChFrame")
        .def_readwrite("time", &chframe::time)
        .def_readwrite("vals", &chframe::vals)
    ;

    class_<std::vector<float> >("stl_vector_float")
        .def(vector_indexing_suite<std::vector<float> >())
    ;

    class_<std::vector<chframe> >("stl_vector_chframe")
        .def(vector_indexing_suite<std::vector<chframe> >())
    ;
 
    class_<DRS4Wrapper>("DRS4Wrapper")
        .def("initBoard", &DRS4Wrapper::initBoard)

        .def("SetFrequency", &DRS4Wrapper::SetFrequency)
        .def("SetTriggerChannel", &DRS4Wrapper::SetTriggerChannel)
        .def("SetTriggerPolarity", &DRS4Wrapper::SetTriggerPolarity)
        .def("SetTriggerLevel", &DRS4Wrapper::SetTriggerLevel)

        .def("GetFrequency", &DRS4Wrapper::GetFrequency)

        .def("record", &DRS4Wrapper::record)
        .def("record2", &DRS4Wrapper::record2)
        .def("getLastFrame", &DRS4Wrapper::getLastFrame)
        .def("getTime", &DRS4Wrapper::getTime)

        .def("measureLevel", &DRS4Wrapper::measureLevel)
        .def("measurePeak", &DRS4Wrapper::measurePeak)
        .def("measureNPeak", &DRS4Wrapper::measureNPeak)
        .def("measurePeakPeak", &DRS4Wrapper::measurePeakPeak)
        .def("measureRMS", &DRS4Wrapper::measureRMS)
        .def("measureCharge", &DRS4Wrapper::measureCharge)
        .def("measureFrequency", &DRS4Wrapper::measureFrequency)
        .def("measurePeriod", &DRS4Wrapper::measurePeriod)
        .def("measureRise", &DRS4Wrapper::measureRise)
        .def("measureFall", &DRS4Wrapper::measureFall)
        .def("measurePosWidth", &DRS4Wrapper::measurePosWidth)
        .def("measureNegWidth", &DRS4Wrapper::measureNegWidth)
        .def("measureDelay", &DRS4Wrapper::measureDelay)
    ;
};


int main() {
    return 0;
}