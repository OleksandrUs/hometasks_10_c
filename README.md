**Description**

*Home tasks are:*

• Create tasks and delete.<br>
• Assign task priority change priority.<br>
• Create idle task hook.<br>

**Requirements**

Keil uVision v5.35.0.0<br>
STM32CubeMX v6.3.0<br>

**Notes**

There are two tasks created in the program from the very begining. The first task is used 
to read periodically the state of the user button. The second task calculates the CPU load
and this calculation in based on the task profiler values. The idle task hook is also implemented.
Each time the user button is pressed, a new task to control LEDs (toggle LEDs state) is created if
it hasn't exist or the task is deleted if it has already been created. If a task is active,
the color LEDs is blinking.

**Demonstration**
<br>
![](result.gif)
