Module Module1


    Dim intel_hex_array(30720) As Byte
    Dim port As System.IO.Ports.SerialPort
    Dim page_size As Integer = 128 'For ATmega168, we have to load 128 bytes at a time (program full page). Document: AVR095
    Dim baudRate As Integer = 4800
    Dim comPort As String = "COM5"
    Dim hexPath As String = "input.hex"
    Dim IrPeriod As Integer = 26
    Dim verboseMode As Boolean = False
    Dim quiteMode As Boolean = False
    Dim automaticReset As Boolean = False
    Dim useParityBit As Boolean = False
    Dim USBMode As Boolean = False


    ' Display a list of commands to the screen
    Sub PrintHelp()
        Console.WriteLine("Atmega328 OTA IT Programming")
        Console.WriteLine("")
        Console.WriteLine("-P        Enables an Even parity bit")
        Console.WriteLine("-b        Baud Rate")
        Console.WriteLine("-p        Com Port")
        Console.WriteLine("-i        IR Carrior Period")
        Console.WriteLine("-f        Input Hex file")
        Console.WriteLine("-a        Automatic Reset")
        Console.WriteLine("-u        USB mode - no IR")
        Console.WriteLine("-v        Verbose - Output more info")
        Console.WriteLine("-q        Quite - Supress fancy progress bar")
        Console.WriteLine("-h        This help")
        Console.WriteLine("")
        Console.WriteLine("Example:")
        Console.WriteLine("AvrDudeIR -b 4800 -p COM5 -f Sketch.hex")
    End Sub

    Sub Main()

        ' Get a list of command arguments
        Dim args() As String = System.Environment.GetCommandLineArgs()

        'If not commands provided then show the help
        If args.Length = 1 Then
            PrintHelp()
            End
        End If

        ' Change the default paramets based on the provided commands
        Try
            Dim i As Integer = 1
            While (i < args.Length - 1)
                If args(i) = "-b" Then
                    baudRate = args(i + 1)
                ElseIf args(i) = "-p" Then
                    comPort = args(i + 1)
                ElseIf args(i) = "-i" Then
                    IrPeriod = args(i + 1)
                ElseIf args(i) = "-f" Then
                    hexPath = args(i + 1)
                ElseIf args(i) = "-h" Then
                    PrintHelp()
                ElseIf args(i) = "-v" Then
                    verboseMode = True
                    i = i - 1
                ElseIf args(i) = "-q" Then
                    quiteMode = True
                    i = i - 1
                ElseIf args(i) = "-a" Then
                    automaticReset = True
                    i = i - 1
                ElseIf args(i) = "-P" Then
                    useParityBit = True
                    i = i - 1
                ElseIf args(i) = "-u" Then
                    USBMode = True
                    i = i - 1
                Else
                    Console.ForegroundColor = ConsoleColor.Red
                    Console.WriteLine("Unknown parameter: " & args(i))
                    Console.ForegroundColor = ConsoleColor.Gray
                    i = i - 1
                End If
                i = i + 2
            End While
        Catch ex As Exception
            Console.WriteLine("Unspecified parameter")
            Console.WriteLine(ex.Message)
        End Try

        Console.WriteLine("IR Programing comencing")
        Console.WriteLine()

        If verboseMode Then
            Console.WriteLine()
            Console.WriteLine("COM Port: " & comPort)
            Console.WriteLine("Hex File: " & hexPath)
            Console.WriteLine("IR Period: " & IrPeriod & "us")
            Console.WriteLine("IR Frequency: " & FormatNumber(1 / (IrPeriod * 0.001), 2) & "kHz")
            Console.WriteLine("Baud Rate: " & baudRate)
            Console.WriteLine("Parity Bit: " & useParityBit)
            Console.WriteLine("Automatic Reset: " & automaticReset)
            Console.WriteLine("USB Mode: " & USBMode)
            Console.WriteLine("Qutie Mode: " & quiteMode)
            Console.WriteLine("Verbose Mode: " & verboseMode)
            Console.WriteLine()
        End If
        Console.Write("Reading hex file...")
        Try
            parse_file(hexPath)
        Catch ex As Exception
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("Failed")
            Console.WriteLine(ex.Message)
            End
        End Try
        Console.WriteLine("Done")

        Console.Write("Opening port...")
        Try
            port = New IO.Ports.SerialPort(comPort)
            port.ReadTimeout = 3000
            port.BaudRate = 4800
            port.StopBits = 1
            If USBMode Then
                port.Parity = IO.Ports.Parity.Even
            Else
                port.Parity = IO.Ports.Parity.None
            End If
            port.DataBits = 8
            port.Handshake = IO.Ports.Handshake.None
            If port.IsOpen = False Then
                port.Open()
            End If
            Console.WriteLine("Open")
        Catch ex As Exception
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("Failed")
            Console.WriteLine(ex.Message)
            End
        End Try

        ' Validate the provided baud rate
        ' this is limited by 15 bits

        If baudRate > 32767 Then
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("Specified baud rate is too high, bust be less than 32700")
            End
        End If

        'If using usb overide baud rate
        If USBMode Then
            Console.WriteLine("USB progrtamming overiding baud rate to 115200")
            baudRate = 115200
        End If

        'Setup the first 3 bytes to send
        'IR carrior frequency
        'Baud rate
        'Pairty bit



        If USBMode Then

            ' Put the micro into reset
            port.DtrEnable = True

            'Both setup bytes equal 1 indicates usb mode
            Dim setupData(2) As Byte
            setupData(1) = 1
            setupData(2) = 1

            ' Release the micro from reset
            port.DtrEnable = False
            ' Keep the micro in bootloader
            keepMicroInBootloader()

            port.DiscardOutBuffer()
            ' Send configuate for usb mode
            port.Write(setupData, 1, 2)
            System.Threading.Thread.Sleep(160)
        Else


            Dim setupData(2) As Byte
            setupData(0) = IrPeriod

            If useParityBit Then
                setupData(1) = Math.Floor(baudRate / 256) + 128
            Else
                setupData(1) = Math.Floor(baudRate / 256)
            End If
            setupData(2) = baudRate Mod 256

            'Try three times to connect to the program incase its stuck
            For retryCount = 0 To 3
                Try
                    port.Write(setupData, 0, 3) ' Send setup data
                    Console.Write("Waiting for programmer response...")

                    'Compaire the recived data to sent data to check a correct connection
                    Dim response As Byte = port.ReadByte()
                    Dim resposeBaudHigh As Byte = port.ReadByte()
                    Dim resposeBaudLow As Byte = port.ReadByte()
                    Dim responseParity As Byte = port.ReadByte()
                    Dim responseBaud As Integer = resposeBaudHigh * 256 + resposeBaudLow
                    If (response <> IrPeriod) Or responseBaud <> baudRate Or CBool(responseParity) <> useParityBit Then ' Check we recive back the period so the conenction is good
                        Console.ForegroundColor = ConsoleColor.Red
                        Console.WriteLine("Invalid programmer response, exiting...")
                        End
                    End If
                    Exit For
                Catch ex As Exception
                    Console.ForegroundColor = ConsoleColor.Red
                    Console.WriteLine("Programmer not responding")
                    Console.WriteLine(ex.Message)
                    If retryCount = 3 Then
                        End
                    End If
                End Try
            Next

        End If


        ' Change the baud rate to the specified rate for programming
        Try
            port.BaudRate = baudRate
        Catch ex As Exception
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("Failed to change baud rate")
            Console.WriteLine(ex.Message)
            End
        End Try

        Console.WriteLine("Done")
        Console.WriteLine()



        If USBMode Then
            port.DiscardOutBuffer()
            keepMicroInBootloader()
            port.DiscardOutBuffer()
            System.Threading.Thread.Sleep(160)
        Else
            ' If automatticly resetting then dont do anything else pause with a message box
            If automaticReset Then
                Console.ForegroundColor = ConsoleColor.Green
                Console.WriteLine("Hopefully automaticly reset, starting programming")
                Console.ForegroundColor = ConsoleColor.Gray
                System.Threading.Thread.Sleep(200)
            Else
                Console.ForegroundColor = ConsoleColor.Green
                Console.WriteLine("Reset the target and press OK to begin programing")
                Console.ForegroundColor = ConsoleColor.Gray
                MsgBox("Reset the target and press OK to begin programing", MsgBoxStyle.Information, "Ready to program")
            End If
        End If


        ' Send a start byte to incidate programming
        Dim temC(1) As Byte

        If Not USBMode Then
            temC(0) = Asc("s")
            port.Write(temC, 0, 1) ' Send s period for Ready
        End If


        Dim Memory_Address_High As Integer
        Dim Memory_Address_Low As Integer

        Dim Check_Sum As Integer

        Dim current_memory_address As Integer
        Dim last_memory_address As Integer
        'Find the last spot in the large hex_array
        last_memory_address = 30720 - 1 'Max memory size needs to be set as a define
        Do While last_memory_address > 0
            If intel_hex_array(last_memory_address) <> 255 Then Exit Do
            last_memory_address = last_memory_address - 1
        Loop

        Console.WriteLine("Programming...")

        Dim startTime As DateTime = Now

        'Start at beginning of large intel array
        current_memory_address = 0
        Do While current_memory_address <last_memory_address

            ' Supress moving of console cursor
            If quiteMode Then
                Console.WriteLine()
                Console.WriteLine()
            Else
                Console.CursorLeft = 0
            End If

            ' Dispay a progress bar
            Console.Write("Writing : ")
            Dim percent As Double = (current_memory_address / last_memory_address) * 100
            For i = 0 To 50
                If (i * 2 < percent) Then
                    Console.Write("#")
                Else
                    Console.Write(" ")
                End If
            Next
            Dim percentStr As String = FormatNumber(percent, 1)
            Dim elapsed As Double = Math.Round(Now.Subtract(startTime).TotalSeconds, 2)

            Console.Write(" : " & percentStr & "%    ")
            '  Console.CursorLeft = 
            Console.Write(elapsed & "s")

            '=============================================
            'Convert 16-bit current_memory_address into two 8-bit characters
            Memory_Address_High = Int(current_memory_address / 256)
            Memory_Address_Low = (current_memory_address Mod 256)

            '=============================================
            'Calculate current check_sum
            Check_Sum = 0
            Check_Sum = Check_Sum + page_size
            Check_Sum = Check_Sum + Int(Memory_Address_High) 'Convert high byte
            Check_Sum = Check_Sum + Int(Memory_Address_Low) 'Convert low byte

            For j = 0 To (page_size - 1)
                Check_Sum = Check_Sum + intel_hex_array(current_memory_address + j)
            Next

            'Now reduce check_sum to 8 bits
            Do While Check_Sum > 256
                Check_Sum = Check_Sum - 256
            Loop
            'Now take 2's compliment
            Check_Sum = 256 - Check_Sum

            '=============================================
            Dim sendByte(0) As Byte

            'Send the start character
            SendByteLimit(CByte(Asc(":")))

            'Send the record length
            SendByteLimit(page_size)

            'Send this block's address
            SendByteLimit(Memory_Address_Low)
            SendByteLimit(Memory_Address_High)

            'Send this block's check sum
            SendByteLimit(Check_Sum)

            'Send the block
            For j = 0 To (page_size - 1)
                SendByteLimit(intel_hex_array(current_memory_address + j))
            Next
            '=============================================

            ' When using usb mode wait at the end of the page for the micro to request the next
            If USBMode Then
                Try
                    port.ReadByte()
                Catch ex As Exception
                    Console.ForegroundColor = ConsoleColor.Red
                    Console.WriteLine("Micro never requested next page, programming failed")
                    End
                End Try
            End If

            current_memory_address = current_memory_address + page_size
        Loop

        'Tell the target IC we are done transmitting data
        SendByteLimit(CByte(Asc(":")))
        SendByteLimit(CByte(Asc("S")))

        If quiteMode Then
            Console.WriteLine()
            Console.WriteLine()
        Else
            Console.CursorLeft = 0
        End If

        ' Display a compete progress bar

        Console.Write("Writing : ##################################################  : 100%    ")
        Console.WriteLine(Math.Round(Now.Subtract(startTime).TotalSeconds, 2) & "s        ")
        Console.WriteLine()
        Console.ForegroundColor = ConsoleColor.Green

        Console.WriteLine("Done uploading, lets hope it worked...")
        Console.ForegroundColor = ConsoleColor.Gray


        ' close com port
        If port.IsOpen Then
            port.Close()
        End If


    End Sub

    Sub keepMicroInBootloader()
        Dim temC(1) As Byte
        For x = 0 To 100
            temC(0) = &HAA
            port.Write(temC, 0, 1)
        Next
    End Sub

    Dim bytesSent As Integer = 0

    'Sends byte to thew com port but pauses after 100 bytes until the programmer requests more data
    Sub SendByteLimit(ByVal b As Byte)
        ' No send limit on USB mode
        If Not USBMode And bytesSent > 100 Then
            Try
                Dim response As Byte = port.ReadByte()
            Catch ex As Exception
                Console.ForegroundColor = ConsoleColor.Red
                Console.WriteLine("Programmer not responding, didnt request more data")
                End
            End Try

            bytesSent = 0
        End If
        Dim sendByte(0) As Byte
        sendByte(0) = b
        port.Write(sendByte, 0, 1)
        bytesSent += 1

    End Sub

    'Decodes a hex file into binary pages
    Private Function parse_file(ByRef filepath As String) As Boolean

        Dim temp_text As String
        Dim Record_Length As Integer
        Dim i As Integer
        Dim x As Integer

        Dim Memory_Address_High As String
        Dim Memory_Address_Low As String
        Dim Memory_Address As Integer

        Dim End_Record As String

        Dim fnum As Integer
        Dim fileContentLines() As String

        parse_file = False 'Indicate we have not parsed file correctly yet

        'Read in the HEX file - clean it up
        Dim hexFile As String = My.Computer.FileSystem.ReadAllText(filepath)

        If hexFile <> "" Then

            'Read each line into fileContentLines() array
            fileContentLines = Split(hexFile, vbCrLf)

            'Remove empty lines + lines not starting with a ":"
            For i = 0 To UBound(fileContentLines)
                If Len(fileContentLines(i)) = 0 Then fileContentLines(i) = vbNullChar
                If Left(fileContentLines(i), 1) <> ":" Then fileContentLines(i) = vbNullChar
                If Mid(fileContentLines(i), 8, 2) = "04" Then fileContentLines(i) = vbNullChar
            Next

            fileContentLines = Filter(fileContentLines, vbNullChar, False)
            'totalLen = UBound(fileContentLines)

        Else
            MsgBox("Error, no HEX file found", vbOKOnly)
            Exit Function
        End If

        'Pre-fill hex_array with 0xFF
        For i = 0 To 30720
            intel_hex_array(i) = 255
        Next i

        'Step through cleaned file and load file contents into large array
        'Also convert from ASCII to binary
        For i = 0 To UBound(fileContentLines)

            temp_text = fileContentLines(i)

            'Peel off ':'
            temp_text = Right(temp_text, Len(temp_text) - 1)

            'Peel off record length
            Record_Length = Hex_Convert(Left(temp_text, 2))
            temp_text = Right(temp_text, Len(temp_text) - 2)

            'Get the memory address of this line
            Memory_Address_High = Hex_Convert(Left(temp_text, 2))
            temp_text = Right(temp_text, Len(temp_text) - 2)
            Memory_Address_Low = Hex_Convert(Left(temp_text, 2))
            temp_text = Right(temp_text, Len(temp_text) - 2)

            Memory_Address = (Memory_Address_High * 256) + Memory_Address_Low

            'Divide Memory address by 2
            '        If (Memory_Address_High Mod 2) <> 0 Then
            '            Memory_Address_Low = Memory_Address_Low + 256
            '        End If
            '        Memory_Address_High = Memory_Address_High \ 2
            '        Memory_Address_Low = Memory_Address_Low \ 2

            'Peel off and check for end of file tage
            End_Record = Hex_Convert(Left(temp_text, 2))
            If End_Record = 1 Then Exit For
            temp_text = Right(temp_text, Len(temp_text) - 2)

            For x = 0 To Record_Length
                'Pull out the byte and store it in the memory_address location of the intel_hex array
                intel_hex_array(Memory_Address + x) = Hex_Convert(Left(temp_text, 2))
                temp_text = Right(temp_text, Len(temp_text) - 2)
            Next x

        Next i 'Goto next line

        parse_file = True 'Indicate we have sucessfully parsed file

    End Function

    Private Function Hex_Convert(ByVal nate As String) As Integer
        'This function takes a two character string and converts it to an integer
        Dim new_hex As Long
        Dim temp As Integer
        Dim i As Integer

        new_hex = 0

        For i = 0 To nate.Length - 1

            'Peel off first letter
            temp = Asc(Right(Left(nate, 1 + i), 1))

            'Convert it to a number
            If temp >= Asc("A") And temp <= Asc("F") Then
                temp = temp - Asc("A") + 10
            ElseIf temp >= Asc("0") And temp <= Asc("9") Then
                temp = temp - Asc("0")
            End If

            'Shift the number
            new_hex = new_hex * 16 + temp

        Next

        Return new_hex

    End Function


End Module
