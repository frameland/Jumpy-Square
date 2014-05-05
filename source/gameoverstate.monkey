Strict
Import game

Class GameOverState


'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------	
	Method New(game:GameScene)
		Self.game = game
		font = FontCache.GetFont(RealPath("font"))
		MedalItem.font = font
		
		bgY = Vsat.ScreenHeight * 0.15 + font.height * 1.5
		bgHeight = Vsat.ScreenHeight * 0.3
		
		playAgainText = "Play Again"
		earnedMedalsText = "Earned Medals"
		
		glowImage = ImageCache.GetImage(RealPath("glow.png"))
		glowImage.SetHandle(0, glowImage.Height()/2)
	End
	
	Method Activate:Void()
		active = True
		time = 0.0
		currentPosX = 0.0
		targetPosX = -99999
		InitMedals()
	End
	
	Method InitMedals:Void()
		Local items:String[] = Medals.MedalsEarnedThisRound()
		medalItems = New MedalItem[items.Length/2]
		For Local i:Int = 0 Until medalItems.Length
			Local index1:Int = i * 2
			Local index2:Int = index1 + 1
			medalItems[i] = New CustomMedalItem(items[index1], Int(items[index2]))
			medalItems[i].position.y = bgY + (bgHeight - medalItems[i].Height * 0.9) * 0.5
			medalItems[i].position.x = ((i+1) * Vsat.ScreenWidth * 0.3) - Vsat.ScreenWidth*0.1
			medalItems[i].SetScale(0.9)
		Next
		If medalItems.Length = 1
			medalItems[0].position.x = Vsat.ScreenWidth2
		ElseIf medalItems.Length = 2
			medalItems[0].position.x = Vsat.ScreenWidth * 0.35
			medalItems[1].position.x = Vsat.ScreenWidth * 0.65
		End
		
	End
	

'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------	
	Method Update:Void(dt:Float)
		If Not active Return
		
		time += dt
		If game.UsedActionKey() And game.backButton.isDown = False
			If game.backgroundColor.Equals(game.gameOverColor)
				If TouchY() > Vsat.ScreenHeight * 0.7
					ReturnToGame()
				End
			End
		End
		
		UpdateSwiping(dt)
	End
	
	Method UpdateSwiping:Void(dt:Float)
		If TouchDown() Return
		If targetPosX <> -99999
			Local lerp:Float = 0.1
			currentPosX += (targetPosX - currentPosX) * lerp
			If Abs(targetPosX - currentPosX) < 0.01
				currentPosX = targetPosX
				targetPosX = -99999
			End
		End
	End
	


'--------------------------------------------------------------------------
' * Render
'--------------------------------------------------------------------------
	Method Render:Void()
		If Not active Return
		ResetBlend()
		
		PushMatrix()
			Local scale:Float = 2.0 - time * 1/0.25
			scale = Max(1.0, scale)
			ScaleAt(Vsat.ScreenWidth2, Vsat.ScreenHeight2, scale, scale)
			RenderMedalsBackground()
			RenderMedals()
			RenderPlayAgain()
		PopMatrix()
	End
	
	Method RenderPlayAgain:Void()
		Color.Orange.Use()
		SetAlpha(globalAlpha.Alpha)
		font.DrawText(playAgainText, Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.7, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
	End
	
	Method RenderMedalsBackground:Void()
		Color.Black.UseWithoutAlpha()
		SetAlpha(globalAlpha.Alpha * 0.4)
		DrawRect(0, bgY+1, Vsat.ScreenWidth, bgHeight-2)
		
		Color.White.Use()
		SetAlpha(globalAlpha.Alpha)
		PushMatrix()
			Scale(Vsat.ScreenWidth/glowImage.Width(), 1.0)
			DrawImage(glowImage, 0, bgY)
			DrawImage(glowImage, 0, bgY + bgHeight)
		PopMatrix()
		font.DrawText(earnedMedalsText, Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.15, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
		
		DrawRect(0, bgY, Vsat.ScreenWidth, 1)
		DrawRect(0, bgY + bgHeight, Vsat.ScreenWidth, 1)
	End
	
	Method RenderMedals:Void()
		PushMatrix()
		Translate(-currentPosX, 0)
		For Local i:Int = 0 Until medalItems.Length
			medalItems[i].Render()
		Next
		PopMatrix()
	End
	
	
'--------------------------------------------------------------------------
' * Events
'--------------------------------------------------------------------------
	Method ReturnToGame:Void()
		If Vsat.IsChangingScenes() Return
			
		Audio.PlaySound(Audio.GetSound("audio/fadeout.mp3"), 1)
		active = False
		Local event:= New VEvent
		event.id = "ReturnToGame"
		Vsat.FireEvent(event)
	End
	
	Method OnMouseUp:Void()
		If touchStartX = -99999 Return
			
		Local cursor:Vec2 = New Vec2(TouchX(), TouchY())
		
		touchEndX = touchStartX + (startCursorX - cursor.x)
		If touchEndX = touchStartX
			OnClick(cursor.x, cursor.y)
			Return
		End
		
		touchTime = Vsat.Seconds - touchStartTime
		Local distance:Float = touchEndX - touchStartX
		Local speed:Float = distance / touchTime
		
		targetPosX = currentPosX + Sgn(speed) * Vsat.ScreenWidth2
		Local w:Int
		If medalItems.Length <= 2 And medalItems.Length > 0
			targetPosX = 0
		ElseIf medalItems.Length > 2
			targetPosX = Max(0.0, targetPosX)
			w = medalItems[medalItems.Length-1].Width
			targetPosX = Min(Vsat.ScreenWidth * 0.3 * medalItems.Length - Vsat.ScreenWidth * 0.65 - w, targetPosX)
		End
		
		touchStartX = -99999
	End
	
	Method OnMouseDown:Void()
		Local cursor:Vec2 = New Vec2(TouchX(), TouchY())
		
		If cursor.y > bgY And cursor.y < bgY + bgHeight
			If touchStartX = -99999
				touchStartX = currentPosX
				startCursorX = cursor.x
				touchStartTime = Vsat.Seconds
			Else
				Local diff:Float = startCursorX - cursor.x
				currentPosX = touchStartX + diff
			End
		End
	End
	
	Method OnClick:Void(x:Float, y:Float)
		
	End
	
	
	Private
	Field game:GameScene
	Field shouldReturn:Bool
	Field font:AngelFont
	Field active:Bool
	Field time:Float
	
	Field playAgainText:String
	Field earnedMedalsText:String
	
	Field bgY:Int
	Field bgHeight:Int
	
	Field medalItems:MedalItem[]
	Field currentPosX:Float
	Field touchStartX:Float = -99999
	Field startCursorX:Float
	Field touchEndX:Float
	Field touchStartTime:Float
	Field touchTime:Float
	Field targetPosX:Float
	
	Field glowImage:Image
End



Private
Class CustomMedalItem Extends MedalItem

	Method New(name:String, times:Int)
		Super.New(name)
		Times = times
		color.Set(Color.White)
	End
	
	Method Draw:Void()
		SetAlpha(color.Alpha * globalAlpha.Alpha)
		Super.Draw()
	End
	
	Method WasTouched:Bool(cursor:Vec2)
		Return PointInRect(cursor.x, cursor.y, position.x-Width/2, position.y, Width, Height)
	End
	
End
