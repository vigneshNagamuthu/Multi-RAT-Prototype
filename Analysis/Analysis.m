clear all
close all
clc

measurementDate = '2025-08-11';

%---------------------- plot singte tcp ---------------------------------%
dir = 'C:\Users\A103764\OneDrive - Singapore Institute Of Technology\Vignesh_OneDrive\QualipocScannerResults\LazarusCampaignMC5\TCPSingtel\iperf3_results_20250811_145630.csv';
file = readtable(dir);

timeStrings = string(file.Var1);
tcp.timestamp = datetime( measurementDate + " " + timeStrings, 'InputFormat', 'yyyy-MM-dd HH:mm:ss');

tcp.iperf3_Mbps = file.Var2;
tcp.lat = file.Var5;
tcp.long = file.Var6;




% -------------------- plot m1 tcp ---------------------------------------%
dir1 = 'C:\Users\A103764\OneDrive - Singapore Institute Of Technology\Vignesh_OneDrive\QualipocScannerResults\LazarusCampaignMC5\MPTCPDefault\iperf3_results_20250811_144524.csv';
dir2 = 'C:\Users\A103764\OneDrive - Singapore Institute Of Technology\Vignesh_OneDrive\QualipocScannerResults\LazarusCampaignMC5\MPTCPDefault\iperf3_results_20250811_154251.csv';
dir3 = 'C:\Users\A103764\OneDrive - Singapore Institute Of Technology\Vignesh_OneDrive\QualipocScannerResults\LazarusCampaignMC5\MPTCPDefault\iperf3_results_20250811_160059.csv';

file1 = readtable(dir1);
file2 = readtable(dir2);
file3 = readtable(dir3);
mptcpDef = [file1; file2; file3];

timeStrings = string(mptcpDef.timestamp);
mptcpDef.timestamp = datetime( measurementDate + " " + timeStrings, 'InputFormat', 'yyyy-MM-dd HH:mm:ss');

% plot individual wans
figure()

plot(mptcpDef.timestamp, mptcpDef.iperf3_Mbps, 'r'); % MPTCP
hold on
plot(mptcpDef.timestamp, mptcpDef.TX1_Mbps, 'b'); % Wan1
plot(mptcpDef.timestamp, mptcpDef.TX2_Mbps, 'g'); % Wan1
hold off

xlabel('Time');
ylabel('Throughput (Mbps)');
grid on;
legend('Singtel + Gomo (MPTCP,default)', 'Singtel', 'Gomo', 'Location', 'best');
title('Throughput Comparison');

% -----------------------------------------------------------------------%
% % plot together
% figure()
% 
% plot(tcp.timestamp, tcp.iperf3_Mbps, 'r'); % TCP
% hold on
% plot(mptcpDef.timestamp, mptcpDef.iperf3_Mbps, 'b'); % MPTCP
% hold off
% 
% xlabel('Time');
% ylabel('Throughput (Mbps)');
% grid on;
% legend('Singtel (TCP)', 'Singtel + Gomo (MPTCP,default)', 'Location', 'best');
% title('Throughput Comparison');


% -------------------------M1 KPI-------------------------------------- %
dir = 'C:\Users\A103764\OneDrive - Singapore Institute Of Technology\Vignesh_OneDrive\QualipocScannerResults\LazarusCampaignMC5\Qualipoc\M1MC5.csv';
file = readtable(dir);

% extract lat, long, 
