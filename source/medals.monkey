Strict
Import vsat
Import game


Class Medals

'--------------------------------------------------------------------------
' * Medals
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


'--------------------------------------------------------------------------
' * Config Vars
'--------------------------------------------------------------------------
	Global DoubleDodgeTime:Float = 1.0
	Global TripleDodgeTime:Float = 2.0
	Global MultiDodgeTime:Float = 2.8


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
		HalfDead = keyValue.GetInt("Half-Dead")
		Feeder = keyValue.GetInt("Feeder")
	End
	

'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------
	Function Update:Void(gameScene:GameScene)
		If gameScene.gameOver Return
		
		'Setting vars
		Local player:Player = gameScene.player
		MedalState.PreviousHighscore = gameScene.Highscore
		
		'Half Dead
		If player.position.y + player.size.y * 0.4 > Vsat.ScreenHeight
			MedalState.CouldGoHalfDead = True
		ElseIf MedalState.CouldGoHalfDead
			MedalState.CouldGoHalfDead = False
			HalfDead += 1
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
		NormalDodge += 1
		FireEvent("Normal-Dodge")
		
		'Close One
		If MedalState.CouldBeClose
			CloseOne += 1
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
				MultiDodge += 1
				FireEvent("Multi-Dodge")
			End
			If a - c < TripleDodgeTime
				TripleDodge += 1
				FireEvent("Triple-Dodge")
			End
			If a - b < DoubleDodgeTime
				DoubleDodge += 1
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
				TripleDodge += 1
				FireEvent("Triple-Dodge")
			End
			If a - b < DoubleDodgeTime
				DoubleDodge += 1
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
				DoubleDodge += 1
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
				NotSurprised += 1
				FireEvent("Not Surprised")
			End
		Next
		
		'Scoreman
		If MedalState.PreviousHighscore < gameScene.score And gameScene.scoreMannedThisRound = False
			Scoreman += 1
			FireEvent("Scoreman")
		End
		
	End
	
	Function UpdatePostGame:Void(gameScene:GameScene)
		If Not gameScene.gameOver Return
		
		'Set vars
		MedalState.LastScoreTime.Clear()
		MedalState.LastScore = 0
		MedalState.CouldGoHalfDead = False
		MedalState.CouldBeClose = False
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
			Default
				Throw New Exception("False medal name: " + medalName)
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
			Default
				Throw New Exception("Unknown medal name: " + medalName)
		End
	End
	
	Function DescriptionFor:String(medalName:String)
		Select medalName
			Case "Normal-Dodge"
				Return "Dodged an incoming block."
			Case "Double-Dodge"
				Return "Dodged 2 incoming blocks in quick succession."
			Case "Triple-Dodge"
				Return "Dodged 3 incoming blocks in quick succession."
			Case "Multi-Dodge"
				Return "Dodged more than 3 incoming blocks in quick succession."
			Case "Close One"
				Return "Dodged at the last possible moment."
			Case "Half-Dead"
				Return "Jumped while nearly out of the bottom of the screen."
			Case "Not Surprised"
				Return "Dodged a surprise."
			Case "Scoreman"
				Return "Beat your old highscore."
			Case "Feeder"
				Return "Filled the entire scorefeed."
			Default
				Throw New Exception("Unknown medal name: " + medalName)
		End
	End
	
	
'--------------------------------------------------------------------------
' * Private
'--------------------------------------------------------------------------
	Private
	Global MedalEvent:VEvent = New VEvent
	
	Function FireEvent:Void(id:String)
		MedalEvent.id = "medal_" + id
		MedalEvent.x = HowMuchPointsFor(id)
		Vsat.FireEvent(MedalEvent)
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
End









