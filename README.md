# Directory  
Directory one_trigger and one_trigger_update correspond to Chap 4.2  
Directory one_trigger_queue corresponds to the first test in Chap 4.3  
Directory token_buffer, token_everytime, token_global  corresponds to three cases in Chap 4.3  
Directory two_in  corresponds to Chap 4.4  
Directory two_out  corresponds to Chap 4.5  
Directory overall  corresponds to Chap 4.6  
Directory performance  corresponds to Chap 5  
Directory system_loop corresponds to Chap 4.7  


# Issues: system_loop has the latest version  
overall has the latest tokenaware condition but lacks atomic and mutex  
other examples lack all three aspects. (tokenaware condition but lacks atomic and mutex)

## Configure
cmake -S .. -B . -D Holoscan_ROOT="/opt/nvidia/holoscan"
## Build
cmake --build . -j
