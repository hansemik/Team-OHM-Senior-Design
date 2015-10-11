function [ X,Y ] = plotPoints_Loss(transmitPwr,lenTransCoax,lenTransAnt,lenPath,len,lenRecCoax,lenRecAnt,coaxLoss,antGain,pathLoss,totalPathLoss,f)
%UNTITLED3 Summary of this function goes here
%   Detailed explanation goes here
    
    xTransCoax = lenTransCoax;
    xTransAnt = xTransCoax + lenTransAnt;
    xPath = xTransAnt + lenPath;
    xRecAnt = xPath + lenRecAnt;
    xRecCoax = xRecAnt + lenRecCoax;


    yTransCoax = transmitPwr + coaxLoss;
    yTransAnt = yTransCoax + antGain;
    yPath = yTransAnt + totalPathLoss; 
    yRecAnt = yPath + antGain;
    yRecCoax = yRecAnt + coaxLoss;
    
    
    X = [0,xTransCoax,xTransAnt,xPath,xRecAnt,xRecCoax];
    Y = [transmitPwr,yTransCoax,yTransAnt,yPath,yRecAnt,yRecCoax];
    
    %hold on;
    %a = plot(X(1),Y(1),'bx');
    %b = plot(X(2),Y(2));
    %c = plot(X(3),Y(3));
    %d = plot(X(4),Y(4));
    %e = plot(X(5),Y(5));
    %f = plot(X(6),Y(6));
    %figure;
   
    %disp(X);
    %disp(Y);
    
    %disp(X(3));
    %disp(X(4));
    
    xxx = linspace(X(3),X(4),size(len,2));
    %disp(size(xxx));
    
    line([X(1) X(2) X(3)],[Y(1) Y(2) Y(3)]);
    line([X(4) X(5) X(6)],[Y(4) Y(5) Y(6)]);
    
    plot(xxx,pathLoss + transmitPwr);
    
    if f == 900
        format long g;
        str = strcat('At 900MHz:  ', num2str(Y(6),2),' dB');
        text(X(6),Y(6),str,'HorizontalAlignment','left');
    end
    
    %d = NaN;
    %set(d,'Visible','off');
end

