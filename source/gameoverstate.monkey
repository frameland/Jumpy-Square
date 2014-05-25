Strict
Import game

Class GameOverState Implements VActionEventHandler

'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------	
	Method New(game:GameScene)
		Self.game = game
		font = FontCache.GetFont(RealPath("font"))
		MedalItem.font = font
		
		bgY = Vsat.ScreenHeight * 0.15 + font.height * 1.5
		bgHeight = Vsat.ScreenHeight * 0.3
		
		playAgainText = Localize.GetValue("gameover_play_again")
		earnedMedalsText = Localize.GetValue("gameover_earned_medals")
		
		glowImage = ImageCache.GetImage(RealPath("glow.png"))
		glowImage.SetHandle(0, glowImage.Height()/2)
		
		newHighscore = New VLabel(Localize.GetValue("gameover_new_highscore"))
		newHighscore.SetFont(font)
		newHighscore.color.Set(Color.Yellow)
		newHighscore.color.Alpha = 0.0
		newHighscore.alignHorizontal = AngelFont.ALIGN_RIGHT
		newHighscore.alignVertical = AngelFont.ALIGN_CENTER
		newHighscore.position.Set(Vsat.ScreenWidth * 0.9, game.backButton.position.y + Vsat.ScreenHeight * 0.01)
		
		Local baseUnit:Float = Vsat.ScreenWidth * 0.5
		Local size:Int = Int(baseUnit * 0.01)
		newHighscoreEffect = New ParticleEmitter
		newHighscoreEffect.InitWithSize(30)
		newHighscoreEffect.particleLifeSpan = 1.0
		newHighscoreEffect.position.Set(newHighscore.position.x - newHighscore.size.x/2, 0)
		newHighscoreEffect.positionVariance.Set(newHighscore.size.x/2, 0)
		newHighscoreEffect.size.Set(size, size)
		newHighscoreEffect.endSize.Set(size, size)
		newHighscoreEffect.speed = baseUnit * 0.4
		newHighscoreEffect.speedVariance = baseUnit * 0.2
		newHighscoreEffect.endColor.Alpha = 0.0
		newHighscoreEffect.emissionAngle = 90
		newHighscoreEffect.oneShot = True
		newHighscoreEffect.additiveBlend = True
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
	
	Method AddAction:Void(action:VAction)
		action.AddToList(actions)
		action.SetListener(Self)
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
		
		VAction.UpdateList(actions, dt)
		UpdateSwiping(dt)
		newHighscoreEffect.Update(dt)
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
			newHighscore.Render()
			newHighscoreEffect.Render()
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
			
		newHighscore.SetScale(0.2)
		newHighscore.color.Alpha = 0.0
			
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
	
	Method NewHighscore:Void()
		Local delay:= New VDelayAction(0.6)
		delay.Name = "NewHighscore"
		AddAction(delay)
	End
	
	Method DelayedNewHighscore:Void()
		newHighscore.SetScale(0.0)
		newHighscore.color.Alpha = 0.0
		
		Local fadeIn:= New VFadeToAlphaAction(newHighscore.color, 1.0, 0.5, LINEAR_TWEEN)
		Local fadeOut:= New VFadeToAlphaAction(newHighscore.color, 0.0, 0.8, LINEAR_TWEEN)
		Local scale:= New VVec2ToAction(newHighscore.scale, 1.0, 1.0, 0.5, EASE_OUT_EXPO)
		
		Local sequence:= New VActionSequence
		sequence.AddAction(New VActionGroup([VAction(fadeIn), VAction(scale)]))
		sequence.AddAction(New VDelayAction(0.8))
		sequence.AddAction(fadeOut)
		AddAction(sequence)
		
		newHighscoreEffect.StopNow()
		newHighscoreEffect.Start()
		
		Local sound:= Audio.GetSound("audio/new_highscore.mp3")
		Audio.PlaySound(sound, 27)
	End
	
	Method OnActionEvent:Void(id:Int, action:VAction)
		If id = VAction.FINISHED
			If action.Name = "NewHighscore"
				DelayedNewHighscore()
			End
		End
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
	Field newHighscore:VLabel
	
	Field newHighscoreEffect:ParticleEmitter
	
	Field actions:List<VAction> = New List<VAction>
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
