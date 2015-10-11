clear;
%%%% Initial testing of antennas %%%%

%Free Space Path Loss

%f = 0:5:100; %MHz
%d = 0:5:100; %Meters

d = 0:1:120;
lenTransCoax = 3;
lenTransAnt  = .1;
lenPath      = 300;
lenRecCoax   = 3;
lenRecAnt    = .1;

lenTotal = lenTransCoax + lenTransAnt + lenPath + lenRecCoax + lenRecAnt;

transmitPwr = 30;
coaxLoss = -3;
antGain = 3;
pathLoss = -60;
antSensitivity = -90;

for f = 100:100:1000
    len = .27:.1:lenPath;
    pathLoss = -(20*log10(len) + 20*log10(f) - 27.55);
    totalPathLoss = -(20*log10(lenPath) + 20*log10(f) - 27.55);
    hold on;
    [X,Y] = plotPoints_Loss(transmitPwr,lenTransCoax,lenTransAnt,lenPath,len,lenRecCoax,lenRecAnt,coaxLoss,antGain,pathLoss,totalPathLoss,f);
    plot(get(gca,'xlim'),[antSensitivity antSensitivity]);
    ylabel('Signal Strength (dB)');
    xlabel('Distance (meters)');
    title('Signal Strength vs Distance in Free Space');
end


figure;
%Constant comes from solving FSPL eq with meters and MHz
%FSPL = 20*log10(d) + 20*log10(f) - 27.55;

for f = 100:100:1000
    d = 0:.5:200; %Meters
    FSPL = 20*log10(d) + 20*log10(f) - 27.55;
    plot(d,FSPL); hold on;
end

ylabel('FSPL (dB)');
xlabel('Distance (meters)');
title('FSPL for 5 to 100 MHz');





