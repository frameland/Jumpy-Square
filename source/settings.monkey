Strict
Import vsat
Import game
Import extra
Import audio
Import back


Class SettingsScene Extends VScene

'--------------------------------------------------------------------------
' * Init
'--------------------------------------------------------------------------	
	Method OnInit:Void()
		Self.shouldClearScreen = False
		font = FontCache.GetFont(RealPath("font"))
		
		InitMainMenu()
		InitMusicOnOff(font)
		InitCredits(font)
		
		back = New BackButton
		back.SetFont(RealPath("font2"))
		
		Local transition:= New FadeInTransition(0.2)
		Vsat.StartFadeIn(transition)
	End
	
	Method InitMainMenu:Void()
		Local mainMenu:Object = Vsat.RestoreFromClipboard("MainMenu")
		If mainMenu
			Self.mainMenuObject = MainMenu(mainMenu)
		End
		AssertWithException(mainMenu, "Couldnt find MainMenu in Vsat-Clipboard")
	End
	
	Method InitMusicOnOff:Void(font:AngelFont)
		If Audio.IsMuted()
			music = New VLabel("Music is off:")
		Else
			music = New VLabel("Music is on:")
		End
		music.SetFont(font)
		music.position.x = Vsat.ScreenWidth2 * 0.98
		music.position.y = Vsat.ScreenHeight * 0.2
		music.alignHorizontal = AngelFont.ALIGN_RIGHT
		
		If Audio.IsMuted()
			musicOnOff = New MenuItem("Play")
		Else
			musicOnOff = New MenuItem("Mute")
		End
		musicOnOff.SetFont(font)
		musicOnOff.color.Set(Color.Orange)
		musicOnOff.downColor.Set(New Color(Color.Orange))
		musicOnOff.downColor.Alpha = 0.5
		musicOnOff.position.x = Vsat.ScreenWidth2 * 1.02
		musicOnOff.position.y = music.position.y
		musicOnOff.alignHorizontal = AngelFont.ALIGN_LEFT
	End
	
	Method InitCredits:Void(font:AngelFont)
		creditsMusicTitle = New VLabel("Music")
		creditsMusicTitle.SetFont(font)
		creditsMusicTitle.color.Set(Color.Yellow)
		creditsMusicTitle.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.5)
		creditsMusicTitle.alignHorizontal = True
		
		creditsMusic = New VLabel("Dominique Lufua the first")
		creditsMusic.SetFont(font)
		creditsMusic.color.Set(Color.White)
		creditsMusic.position.Set(Vsat.ScreenWidth2, creditsMusicTitle.position.y + font.height)
		creditsMusic.alignHorizontal = True
		
		creditsSpecialTitle = New VLabel("Special Thanks")
		creditsSpecialTitle.SetFont(font)
		creditsSpecialTitle.color.Set(Color.Yellow)
		creditsSpecialTitle.position.Set(Vsat.ScreenWidth2, creditsMusic.position.y + font.height * 2.5)
		creditsSpecialTitle.alignHorizontal = True
		
		creditsSpecial = New VLabel[3]
		For Local i:Int = 0 Until creditsSpecial.Length
			Select i
				Case 0
					creditsSpecial[0] = New VLabel("Max Gittl")
				Case 1
					creditsSpecial[1] = New VLabel("David Neubauer")
				Case 2
					creditsSpecial[2] = New VLabel("Sebastian Hoehnl")
			End
			creditsSpecial[i].SetFont(font)
			creditsSpecial[i].color.Set(Color.White)
			creditsSpecial[i].position.Set(Vsat.ScreenWidth2, creditsSpecialTitle.position.y + font.height + (i * font.height*0.9))
			creditsSpecial[i].alignHorizontal = True
		Next
		
		
	End
	

'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------
	Method OnUpdate:Void(dt:Float)
		UpdateCursor()
		UpdateAlpha()
	End
	
	Method UpdateAlpha:Void()
		If Vsat.IsChangingScenes()
			music.color.Alpha = globalAlpha.Alpha
			musicOnOff.color.Alpha = globalAlpha.Alpha
			creditsMusicTitle.color.Alpha = globalAlpha.Alpha
			creditsMusic.color.Alpha = globalAlpha.Alpha
			creditsSpecialTitle.color.Alpha = globalAlpha.Alpha
			For Local i:Int = 0 Until creditsSpecial.Length
				creditsSpecial[i].color.Alpha = globalAlpha.Alpha
			Next
		End
	End
	
	Method UpdateCursor:Void()
		If TouchDown()
			OnMouseDown()
			lastTouchDown = True
		Else
			If lastTouchDown
				OnMouseUp()
			End
			lastTouchDown = False
		End
	End
	
	
'--------------------------------------------------------------------------
' * Render
'--------------------------------------------------------------------------
	Method OnRender:Void()
		mainMenuObject.ClearScreen()
		mainMenuObject.RenderParticles()
		mainMenuObject.OnRender()
		RenderBackground()
		
		If Vsat.IsChangingScenes()
			Local scaleAll:Float
			If FadeInTransition(Vsat.transition)
				scaleAll = 2.0 - Vsat.transition.Progress
				globalAlpha.Alpha = Vsat.transition.Progress
			ElseIf FadeOutTransition(Vsat.transition)
				scaleAll = 1.0 + Tweening(EASE_OUT_EXPO, Vsat.transition.Progress, 0.0, 1.0, Vsat.transition.Duration)
				globalAlpha.Alpha = 2.0 - scaleAll
			End
			ScaleAt(Vsat.ScreenWidth2, Vsat.ScreenHeight2, scaleAll, scaleAll)
		End
		
		RenderMusicOnOff()
		RenderCredits()
		back.Render()
	End
	
	Method RenderBackground:Void()
		If Not Vsat.IsChangingScenes()
			Color.NewBlack.UseWithoutAlpha()
			SetAlpha(0.95)
			DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
		End
	End
	
	Method RenderMusicOnOff:Void()
		music.Render()
		musicOnOff.Render()
	End
	
	Method RenderCredits:Void()
		creditsMusicTitle.Render()
		creditsMusic.Render()
		creditsSpecialTitle.Render()
		For Local i:Int = 0 Until creditsSpecial.Length
			creditsSpecial[i].Render()
		Next
	End
	
	
'--------------------------------------------------------------------------
' * Events
'--------------------------------------------------------------------------
	Method OnMouseUp:Void()
		Local cursor:Vec2 = New Vec2(TouchX(), TouchY())
		If back.isDown
			back.isDown = False
			If back.WasTouched(cursor)
				OnCancel()
			End
			Return
		End
		
		If musicOnOff.isDown
			If musicOnOff.WasTouched(cursor)
				OnMusicOnOff()
			End
		End
	End
	
	Method OnMouseDown:Void()
		Local cursor:Vec2 = New Vec2(TouchX(), TouchY())
		If back.WasTouched(cursor)
			back.isDown = True
			Return
		End
		If back.isDown Return
			
		If musicOnOff.WasTouched(cursor)
			musicOnOff.isDown = True
		End
	End
	
	Method OnCancel:Void()
		If Vsat.transition Return
		Local transition:= New FadeOutTransition(0.6)
		Vsat.ChangeSceneWithTransition(mainMenuObject, transition)
		Audio.PlaySound(Audio.GetSound("audio/fadeout.mp3"), 2)
	End
	
	Method OnMusicOnOff:Void()
		If Audio.IsMuted()
			Audio.Unmute()
			music.Text = "Music is on:"
			musicOnOff.Text = "Mute"
		Else
			Audio.Mute()
			music.Text = "Music is off:"
			musicOnOff.Text = "Play"
		End
	End
	
	
	Private
	Field mainMenuObject:MainMenu
	Field lastTouchDown:Bool
	Field font:AngelFont
	Field lockedMenuItem:MenuItem
	
	Field music:VLabel
	Field musicOnOff:MenuItem
	
	Field creditsMusicTitle:VLabel
	Field creditsMusic:VLabel
	Field creditsSpecialTitle:VLabel
	Field creditsSpecial:VLabel[]
	
	Field back:BackButton

End



Private
Class FadeInTransition Extends VTransition
	
	Field startPoint:Float = 0
	
	Method New()
		Super.New()
	End
	
	Method New(duration:Float)
		Super.New(duration)
	End
	
	Method Render:Void()
		PushMatrix()
		ResetMatrix()
		ResetBlend()
		Local progress:Float = Tweening(LINEAR_TWEEN, Time, 0.0, 1.0, Duration)
		SetAlpha(progress * 0.95)
		Color.NewBlack.UseWithoutAlpha()
		DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
		PopMatrix()
	End

End

Class FadeOutTransition Extends VTransition
	
	Field startPoint:Float = 0
	
	Method New()
		Super.New()
	End
	
	Method New(duration:Float)
		Super.New(duration)
	End
	
	Method Render:Void()
		PushMatrix()
		ResetMatrix()
		ResetBlend()
		Local progress:Float = Tweening(LINEAR_TWEEN, Time, 0.0, 1.0, Duration)
		SetAlpha(0.95 - progress * 0.95)
		Color.NewBlack.UseWithoutAlpha()
		DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
		PopMatrix()
	End

End


Class MenuItem Extends VLabel
	
	Field isDown:Bool
	Field downColor:Color = New Color(1.0, 1.0, 1.0, 0.8)
	
	Method New(withText:String)
		Super.New(withText)
		color.Set(Color.White)
	End
	
	Method Draw:Void()
		If isDown
			downColor.Use()
			SetAlpha(downColor.Alpha * globalAlpha.Alpha)
		Else
			SetAlpha(color.Alpha * globalAlpha.Alpha)
		End
		Super.Draw()
	End
	
	Method WasTouched:Bool(cursor:Vec2)
		If alignHorizontal = AngelFont.ALIGN_LEFT
			Return PointInRect(cursor.x, cursor.y, position.x, position.y, size.x, size.y)
		ElseIf alignHorizontal = AngelFont.ALIGN_RIGHT
			Return PointInRect(cursor.x, cursor.y, position.x-size.x, position.y, size.x, size.y)
		ElseIf alignHorizontal = AngelFont.ALIGN_CENTER
			Return PointInRect(cursor.x, cursor.y, position.x-size.x/2, position.y, size.x, size.y)
		End
		Return False
	End
	
End




