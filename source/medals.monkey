Strict
Import vsat
Import game
Import adventure


Class Medals

'--------------------------------------------------------------------------
' * Medal Counters
'--------------------------------------------------------------------------	
	Global NormalDodge:Int
	Global DoubleDodge:Int
	Global TripleDodge:Int
	Global MultiDodge:Int
	Global CloseOne:Int
	Global Scoreman:Int
	Global NotSurprised:Int
	Global HalfDead:Int
	Global Feeder:Int
	Global Tissue:Int
	Global Minimalist:Int
	Global GoGetter:Int
	Global DirectHit:Int
	Global Highflyer:Int
	Global Supporter:Int
		
'--------------------------------------------------------------------------
' * Config Vars
'--------------------------------------------------------------------------
	Global DoubleDodgeTime:Float = 1.0
	Global TripleDodgeTime:Float = 2.0
	Global MultiDodgeTime:Float = 2.8
	Global FeederTime:Float = 2.0
	

'--------------------------------------------------------------------------
' * Init
'--------------------------------------------------------------------------
	Function InitWithKeyValuePair:Void(keyValue:KeyValuePair)
		NormalDodge = keyValue.GetInt("Normal-Dodge")
		DoubleDodge = keyValue.GetInt("Double-Dodge")
		TripleDodge = keyValue.GetInt("Triple-Dodge")
		MultiDodge = keyValue.GetInt("Multi-Dodge")
		CloseOne = keyValue.GetInt("Close-One")
		Scoreman = keyValue.GetInt("Scoreman")
		NotSurprised = keyValue.GetInt("Not Surprised")
		CloseOne = keyValue.GetInt("Close One")
		HalfDead = keyValue.GetInt("Half-Dead")
		Feeder = keyValue.GetInt("Feeder")
		Tissue = keyValue.GetInt("Tissue")
		Minimalist = keyValue.GetInt("Minimalist")
		GoGetter = keyValue.GetInt("Go-Getter")
		DirectHit = keyValue.GetInt("DirectHit")
		Highflyer = keyValue.GetInt("Highflyer")
		
		Supporter = keyValue.GetInt("Supporter")
		If Supporter > 0
			GameScene.IsUnlocked = True
		End
	End
	
	Function KeyValues:String()
		Local returnString:String
		Local newline:String = "~n"
		
		returnString += "Normal-Dodge = " + NormalDodge + newline
		returnString += "Double-Dodge = " + DoubleDodge + newline
		returnString += "Triple-Dodge = " + TripleDodge + newline
		returnString += "Multi-Dodge = " + MultiDodge + newline
		returnString += "Close One = " + CloseOne + newline
		returnString += "Half-Dead = " + HalfDead + newline
		returnString += "Not Surprised = " + NotSurprised + newline
		returnString += "Scoreman = " + Scoreman + newline
		returnString += "Feeder = " + Feeder + newline
		returnString += "Tissue = " + Tissue + newline
		returnString += "Minimalist = " + Minimalist + newline
		returnString += "Go-Getter = " + GoGetter + newline
		returnString += "DirectHit = " + DirectHit + newline
		returnString += "Highflyer = " + Highflyer + newline
		returnString += "Supporter = " + Supporter + newline
		
		Return returnString
	End
	
	Function EmitEvent:Void(id:String)
		FireEvent(id)
	End
	
	
'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------
	Function Update:Void(gameScene:GameScene)
		If gameScene.gameOver Return
		
		'Setting vars
		Local player:Player = gameScene.player
		If AdventureScene(gameScene)
			MedalState.PreviousHighscore = gameScene.HighscoreAdventure
		Else
			MedalState.PreviousHighscore = gameScene.Highscore
		End
		
		'Half Dead
		If player.position.y + player.size.y * 0.4 > Vsat.ScreenHeight
			MedalState.CouldGoHalfDead = True
		ElseIf MedalState.CouldGoHalfDead
			MedalState.CouldGoHalfDead = False
			FireEvent("Half-Dead")
		End
		
		'Double/Triple/Multi Dodge | Not Surprised
		If gameScene.dodged <> MedalState.LastScore
			MedalState.LastScore = gameScene.dodged
			OnScoreChange(gameScene)
		End
		
		'Close One
		MedalState.playerCopy.size.Set(gameScene.player.size)
		For Local e:= EachIn gameScene.enemies
			Local counter:Int
			For Local v:= EachIn gameScene.player.lastPositions
				MedalState.playerCopy.position.Set(v)

				If (e.wasClose = False) And (e.collidedWithPlayer = False) And MedalState.playerCopy.CollidesWith(e)
					e.wasClose = True
					MedalState.CouldBeClose = True
					Exit
				End

				counter += 1
				If counter > gameScene.player.maxPositions/2
					Exit
				End
			Next
		Next
		
	End

	Function OnScoreChange:Void(gameScene:GameScene)
		'Normal Dodge
		FireEvent("Normal-Dodge")
		
		'Close One
		If MedalState.CouldBeClose
			FireEvent("Close One")
			MedalState.CouldBeClose = False
		End
		
		
		'Double/Triple/Multi Dodge
		MedalState.LastScoreTime.Push(Vsat.Seconds)
		If MedalState.LastScoreTime.Length >= 4
			Local a:Float = MedalState.LastScoreTime.Pop()
			Local b:Float = MedalState.LastScoreTime.Pop()
			Local c:Float = MedalState.LastScoreTime.Pop()
			Local d:Float = MedalState.LastScoreTime.Pop()
			Local dodged:Bool = False
			If a - d < MultiDodgeTime
				FireEvent("Multi-Dodge")
			End
			If a - c < TripleDodgeTime
				FireEvent("Triple-Dodge")
			End
			If a - b < DoubleDodgeTime
				FireEvent("Double-Dodge")
			End
			
			If MedalState.LastScoreTime.Length > 30
				MedalState.LastScoreTime.Clear()
			End
			
			MedalState.LastScoreTime.Push(d)
			MedalState.LastScoreTime.Push(c)
			MedalState.LastScoreTime.Push(b)
			MedalState.LastScoreTime.Push(a)
			
		ElseIf MedalState.LastScoreTime.Length >= 3
			Local a:Float = MedalState.LastScoreTime.Pop()
			Local b:Float = MedalState.LastScoreTime.Pop()
			Local c:Float = MedalState.LastScoreTime.Pop()
			Local dodged:Bool = False
			If a - c < TripleDodgeTime
				FireEvent("Triple-Dodge")
			End
			If a - b < DoubleDodgeTime
				FireEvent("Double-Dodge")
			End
			
			If MedalState.LastScoreTime.Length > 30
				MedalState.LastScoreTime.Clear()
			End
			
			MedalState.LastScoreTime.Push(c)
			MedalState.LastScoreTime.Push(b)
			MedalState.LastScoreTime.Push(a)
		
		ElseIf MedalState.LastScoreTime.Length >= 2
			Local a:Float = MedalState.LastScoreTime.Pop()
			Local b:Float = MedalState.LastScoreTime.Pop()
			Local dodged:Bool = False
			If a - b < DoubleDodgeTime
				FireEvent("Double-Dodge")
			End
			
			If MedalState.LastScoreTime.Length > 30
				MedalState.LastScoreTime.Clear()
			End
			
			MedalState.LastScoreTime.Push(b)
			MedalState.LastScoreTime.Push(a)
		End
		
		'Not surprised
		For Local e:= EachIn gameScene.enemies
			If e.isSurprise And e.hasBeenScored
				e.isSurprise = False
				FireEvent("Not Surprised")
			End
		Next
		
		'Scoreman
		If MedalState.PreviousHighscore < gameScene.score And gameScene.scoreMannedThisRound = False
			FireEvent("Scoreman")
		End
		
	End
	
	Function UpdatePostGame:Void(gameScene:GameScene)
		If Not gameScene.gameOver Return
		
		If gameScene.dodged = 0
			FireEvent("Tissue")
		End
		
		If gameScene.dodged > 1
			Local jumpDodgeRatio:Float = gameScene.player.numberOfJumps / Float(gameScene.dodged)
			If jumpDodgeRatio < 2.2
				FireEvent("Minimalist")
			ElseIf jumpDodgeRatio > 2.9
				FireEvent("Go-Getter")
			End
		End
	End

	Function ResetThisRound:Void()
		MedalState.LastScoreTime.Clear()
		MedalState.LastScore = 0
		MedalState.CouldGoHalfDead = False
		MedalState.CouldBeClose = False
		ThisRound.Reset()
	End


'--------------------------------------------------------------------------
' * Helpers
'--------------------------------------------------------------------------
	Function HowManyOf:Int(medalName:String)
		Select medalName
			Case "Normal-Dodge"
				Return NormalDodge
			Case "Double-Dodge"
				Return DoubleDodge
			Case "Triple-Dodge"
				Return TripleDodge
			Case "Multi-Dodge"
				Return MultiDodge
			Case "Close One"
				Return CloseOne
			Case "Half-Dead"
				Return HalfDead
			Case "Not Surprised"
				Return NotSurprised
			Case "Scoreman"
				Return Scoreman
			Case "Feeder"
				Return Feeder
			Case "Tissue"
				Return Tissue
			Case "Minimalist"
				Return Minimalist
			Case "Go-Getter"
				Return GoGetter
			Case "Direct Hit"
				Return DirectHit
			Case "Highflyer"
				Return Highflyer
			Case "Supporter"
				Return Supporter
			Default
				Throw New Exception("Unknown medal name: " + medalName)
		End
	End
	
	Function HowMuchPointsFor:Int(medalName:String)
		Select medalName
			Case "Normal-Dodge"
				Return 5
			Case "Double-Dodge"
				Return 10
			Case "Triple-Dodge"
				Return 15
			Case "Multi-Dodge"
				Return 20
			Case "Close One"
				Return 5
			Case "Half-Dead"
				Return 5
			Case "Not Surprised"
				Return 15
			Case "Scoreman"
				Return 5
			Case "Feeder"
				Return 25
			Case "Highflyer"
				Return 15
			Case "Supporter", "Tissue", "Minimalist", "Go-Getter", "Direct Hit"
				Return 0
			Default
				Throw New Exception("Unknown medal name: " + medalName)
		End
	End
	
	Function FileNameFor:String(medalName:String)
		Select medalName
			Case "Normal-Dodge"
				Return "normal_dodge.png"
			Case "Double-Dodge"
				Return "double_dodge.png"
			Case "Triple-Dodge"
				Return "triple_dodge.png"
			Case "Multi-Dodge"
				Return "multi_dodge.png"
			Case "Close One"
				Return "close_one.png"
			Case "Half-Dead"
				Return "half_dead.png"
			Case "Not Surprised"
				Return "not_surprised.png"
			Case "Scoreman"
				Return "scoreman.png"
			Case "Feeder"
				Return "feeder.png"
			Case "Tissue"
				Return "tissue.png"
			Case "Minimalist"
				Return "minimalist.png"
			Case "Go-Getter"
				Return "go_getter.png"
			Case "Direct Hit"
				Return "direct_hit.png"
			Case "Highflyer"
				Return "highflyer.png"
			Case "Supporter"
				If GameScene.IsUnlocked
					Return "unlocked.png"
				End
				Return "locked.png"
				
			Default
				Throw New Exception("Unknown medal name: " + medalName)
		End
	End
	
	Function DescriptionFor:String(medalName:String)
		Select medalName
			Case "Normal-Dodge"
				Return Localize.GetValue("medal_description_Normal-Dodge")
			Case "Double-Dodge"
				Return Localize.GetValue("medal_description_Double-Dodge")
			Case "Triple-Dodge"
				Return Localize.GetValue("medal_description_Triple-Dodge")
			Case "Multi-Dodge"
				Return Localize.GetValue("medal_description_Multi-Dodge")
			Case "Close One"
				Return Localize.GetValue("medal_description_Close One")
			Case "Half-Dead"
				Return Localize.GetValue("medal_description_Half-Dead")
			Case "Not Surprised"
				Return Localize.GetValue("medal_description_Not Surprised")
			Case "Scoreman"
				Return Localize.GetValue("medal_description_Scoreman")
			Case "Feeder"
				Return Localize.GetValue("medal_description_Feeder")
			Case "Tissue"
				Return Localize.GetValue("medal_description_Tissue")
			Case "Minimalist"
				Return Localize.GetValue("medal_description_Minimalist")
			Case "Go-Getter"
				Return Localize.GetValue("medal_description_Go-Getter")
			Case "Direct Hit"
				Return Localize.GetValue("medal_description_Direct Hit")
			Case "Highflyer"
				Return Localize.GetValue("medal_description_Highflyer")
			Case "Supporter"
				If GameScene.IsUnlocked
					Return Localize.GetValue("medal_description_Supporter")
				End
				Return ""
				
			Default
				Throw New Exception("Unknown medal name: " + medalName)
		End
	End
	
	Function UnlockSupporterMedal:Void()
		Supporter = 1
		GameScene.IsUnlocked = True
	End
	
	Function PercentageComplete:Int()
		Local total:Int = 15
		Local current:Int = 0
		If NormalDodge > 0 Then current += 1
		If DoubleDodge > 0 Then current += 1
		If TripleDodge > 0 Then current += 1
		If MultiDodge > 0 Then current += 1
		If CloseOne > 0 Then current += 1
		If Scoreman > 0 Then current += 1
		If NotSurprised > 0 Then current += 1
		If HalfDead > 0 Then current += 1
		If Feeder > 0 Then current += 1
		If Tissue > 0 Then current += 1
		If Minimalist > 0 Then current += 1
		If GoGetter > 0 Then current += 1
		If DirectHit > 0 Then current += 1
		If Highflyer > 0 Then current += 1
		If Supporter > 0 Then current += 1
			
		Local percent:Float = Float(current) / total
		Return Int(percent * 100)
	End
	
	'returns array with format:
	'MedalId, numberEarned, MedalId, numberEarned, ... (you have to cast numberEarned to Int)
	Function MedalsEarnedThisRound:String[]()
		'Return Cheat()
		
		Local stack:StringStack = New StringStack
		
		If ThisRound.NormalDodge
			stack.Push("Normal-Dodge")
			stack.Push(ThisRound.NormalDodge)
		End
		If ThisRound.DoubleDodge
			stack.Push("Double-Dodge")
			stack.Push(ThisRound.DoubleDodge)
		End
		If ThisRound.TripleDodge
			stack.Push("Triple-Dodge")
			stack.Push(ThisRound.TripleDodge)
		End
		If ThisRound.MultiDodge
			stack.Push("Multi-Dodge")
			stack.Push(ThisRound.MultiDodge)
		End
		If ThisRound.CloseOne
			stack.Push("Close One")
			stack.Push(ThisRound.CloseOne)
		End
		If ThisRound.HalfDead
			stack.Push("Half-Dead")
			stack.Push(ThisRound.HalfDead)
		End
		If ThisRound.Scoreman
			stack.Push("Scoreman")
			stack.Push(ThisRound.Scoreman)
		End
		If ThisRound.NotSurprised
			stack.Push("Not Surprised")
			stack.Push(ThisRound.NotSurprised)
		End
		If ThisRound.Feeder
			stack.Push("Feeder")
			stack.Push(ThisRound.Feeder)
		End
		If ThisRound.Tissue
			stack.Push("Tissue")
			stack.Push(ThisRound.Tissue)
		End
		If ThisRound.Minimalist
			stack.Push("Minimalist")
			stack.Push(ThisRound.Minimalist)
		End
		If ThisRound.GoGetter
			stack.Push("Go-Getter")
			stack.Push(ThisRound.GoGetter)
		End
		If ThisRound.DirectHit
			stack.Push("Direct Hit")
			stack.Push(ThisRound.DirectHit)
		End
		If ThisRound.Highflyer
			stack.Push("Highflyer")
			stack.Push(ThisRound.Highflyer)
		End
		
		Return stack.ToArray()
	End
	
	Function Cheat:String[]()
		Return ["Normal-Dodge", "23", "Double-Dodge", "1", "Close One", "3", "Half-Dead", "1"]
	End
	
	
'--------------------------------------------------------------------------
' * Private
'--------------------------------------------------------------------------
	Private
	Global MedalEvent:VEvent = New VEvent
	
	Function FireEvent:Void(id:String)
		Local shouldCount:Bool = EarnedMedal(id)
		If shouldCount
			MedalEvent.id = "medal_" + id
			MedalEvent.x = HowMuchPointsFor(id)
			Vsat.FireEvent(MedalEvent)
		End
	End
	
	Function EarnedMedal:Bool(id:String)
		Select id
			Case "Normal-Dodge"
				NormalDodge += 1
				ThisRound.NormalDodge += 1
			Case "Double-Dodge"
				If Vsat.Seconds > MedalState.LastDoubleDodge + DoubleDodgeTime
					DoubleDodge += 1
					ThisRound.DoubleDodge += 1
					MedalState.LastDoubleDodge = Vsat.Seconds
				Else
					Return False
				End
				
			Case "Triple-Dodge"
				If Vsat.Seconds > MedalState.LastTripleDodge + TripleDodgeTime
					TripleDodge += 1
					ThisRound.TripleDodge += 1
					MedalState.LastTripleDodge = Vsat.Seconds
				Else
					Return False
				End
				
			Case "Multi-Dodge"
				If Vsat.Seconds > MedalState.LastMultiDodge + MultiDodgeTime
					MultiDodge += 1
					ThisRound.MultiDodge += 1
					MedalState.LastMultiDodge = Vsat.Seconds
				Else
					Return False
				End
				
			Case "Close One"
				CloseOne += 1
				ThisRound.CloseOne += 1
			Case "Half-Dead"
				HalfDead += 1
				ThisRound.HalfDead += 1
			Case "Not Surprised"
				NotSurprised += 1
				ThisRound.NotSurprised += 1
			Case "Scoreman"
				Scoreman += 1
				ThisRound.Scoreman += 1
			Case "Feeder"
				If Vsat.Seconds > MedalState.LastFeeder + FeederTime
					Feeder += 1
					ThisRound.Feeder += 1
					MedalState.LastFeeder = Vsat.Seconds
				Else
					Return False
				End
				
			Case "Tissue"
				Tissue += 1
				ThisRound.Tissue += 1
				Return False
			Case "Minimalist"
				Minimalist += 1
				ThisRound.Minimalist += 1
				Return False
			Case "Go-Getter"
				GoGetter += 1
				ThisRound.GoGetter += 1
				Return False
			Case "Direct Hit"
				DirectHit += 1
				ThisRound.DirectHit += 1
				Return False
			Case "Highflyer"
				Highflyer += 1
				ThisRound.Highflyer += 1
			Case "Supporter"
				Return False
			Default
				Throw New Exception("Unknown medal name: " + id)
		End
		
		Return True
	End
	
	
	
End




Private
Class MedalState
	Global CouldGoHalfDead:Bool
	Global CouldBeClose:Bool
	Global PreviousHighscore:Int
	Global LastScoreTime:FloatStack = New FloatStack
	Global LastScore:Int
	Global playerCopy:VRect = New VRect(0, 0, 1, 1)
	Global LastDoubleDodge:Float
	Global LastTripleDodge:Float
	Global LastMultiDodge:Float
	Global LastFeeder:Float
End

Class ThisRound
	Global NormalDodge:Int
	Global DoubleDodge:Int
	Global TripleDodge:Int
	Global MultiDodge:Int
	Global CloseOne:Int
	Global Scoreman:Int
	Global NotSurprised:Int
	Global HalfDead:Int
	Global Feeder:Int
	Global Tissue:Int
	Global Minimalist:Int
	Global GoGetter:Int
	Global DirectHit:Int
	Global Highflyer:Int
	
	Function Reset:Void()
		NormalDodge = 0
		DoubleDodge = 0
		TripleDodge = 0
	    MultiDodge = 0
		CloseOne = 0
        Scoreman = 0
        NotSurprised = 0
        HalfDead = 0
        Feeder = 0
		Tissue = 0
		Minimalist = 0
		GoGetter = 0
		DirectHit = 0
		Highflyer = 0
	End
	
End




