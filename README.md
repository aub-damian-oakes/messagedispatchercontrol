# messagedisptachercontrol
<sup>32-bit Visual C++ application</sup> <br/>
<sup>Created by Damian Oakes</sup> <br/>
<sup>for Aubuchon Hardware</sup> <br/>

### Purpose
The purpose of this program is to be able to perform a full reboot of message dispatcher in the back office
by either clicking the executable, or by remotely triggering the executable.

### Features
- CPU Limit on Job Object, caps CPU usage of the program at 10%.
- Recursively monitors child processes created by the application being stopped/started.
- Silent execution, is run without creating any new windows.

<sub>This repository includes the Visual Studio solution for access across devices.</sub> <br />
<sub>The backoffice.ico file was taken from Mi9 Retail's Mi9 Store application.</sub>

### Command Line Options
`/start` - Specifies start-only action. <br /> 
`/stop` - Specifies stop-only action.
