Strict
Import vsat
Import game
Import extra
Import audio
Import back
Import save


Class SettingsScene Extends VScene

'--------------------------------------------------------------------------
' * Init
'--------------------------------------------------------------------------	
	Method OnInit:Void()
		Self.shouldClearScreen = False
		font = FontCache.GetFont(RealPath("font"))
		
		InitMainMenu()
		InitLogo()
		InitMusicOnOff(font)
		InitLanguage(font)
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
			music = New Label(Localize.GetValue("settings_music_is_off"))
		Else
			music = New Label(Localize.GetValue("settings_music_is_on"))
		End
		music.SetFont(font)
		music.position.x = Vsat.ScreenWidth2 * 1.04
		music.position.y = Vsat.ScreenHeight * 0.08
		music.alignHorizontal = AngelFont.ALIGN_RIGHT
		
		If Audio.IsMuted()
			musicOnOff = New MenuItem(Localize.GetValue("settings_play"))
		Else
			musicOnOff = New MenuItem(Localize.GetValue("settings_mute"))
		End
		musicOnOff.SetFont(font)
		musicOnOff.color.Set(New Color(Color.Orange))
		musicOnOff.downColor.Set(New Color(Color.Orange))
		musicOnOff.downColor.Alpha = 0.5
		musicOnOff.position.x = Vsat.ScreenWidth2 * 1.08
		musicOnOff.position.y = music.position.y
		musicOnOff.alignHorizontal = AngelFont.ALIGN_LEFT
	End
	
	Method InitLanguage:Void(font:AngelFont)
		language = New Label(Localize.GetValue("settings_language"))
		language.SetFont(font)
		language.position.Set(music.position)
		language.position.y += font.height * 1.4
		language.alignHorizontal = AngelFont.ALIGN_RIGHT
		
		langs = New MenuItem[2]
		langs[0] = New MenuItem("English")
		langs[1] = New MenuItem("Deutsch")
		For Local i:Int = 0 Until langs.Length
			langs[i].SetFont(font)
			langs[i].position.x = musicOnOff.position.x
			langs[i].position.y = language.position.y + (font.height * i)
			langs[i].downColor.Set(New Color(Color.Orange))
			langs[i].downColor.Alpha = 0.5
		Next
		
		If Localize.GetCurrentLanguage() = "en"
			langs[0].color.Set(New Color(Color.Orange))
			langs[1].color.Set(New Color(Color.White))
		ElseIf Localize.GetCurrentLanguage() = "de"
			langs[0].color.Set(New Color(Color.White))
			langs[1].color.Set(New Color(Color.Orange))
		End
	End
	
	Method InitCredits:Void(font:AngelFont)
		creditsGameTitle = New Label(Localize.GetValue("settings_game_by"))
		creditsGameTitle.SetFont(font)
		creditsGameTitle.color.Set(Color.Yellow)
		creditsGameTitle.position.Set(Vsat.ScreenWidth2, logo.position.y - font.height)
		creditsGameTitle.alignHorizontal = True
		creditsGameTitle.SetScale(0.8)
		
		creditsMusicTitle = New Label(Localize.GetValue("settings_music"))
		creditsMusicTitle.SetFont(font)
		creditsMusicTitle.color.Set(Color.Yellow)
		creditsMusicTitle.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.55)
		creditsMusicTitle.alignHorizontal = True
		creditsMusicTitle.SetScale(0.8)
		
		creditsMusic = New Label("Dominique Lufua")
		creditsMusic.SetFont(font)
		creditsMusic.color.Set(Color.White)
		creditsMusic.position.Set(Vsat.ScreenWidth2, creditsMusicTitle.position.y + font.height)
		creditsMusic.alignHorizontal = True
		creditsMusic.SetScale(0.8)
		
		creditsSpecialTitle = New Label(Localize.GetValue("settings_thanks"))
		creditsSpecialTitle.SetFont(font)
		creditsSpecialTitle.color.Set(Color.Yellow)
		creditsSpecialTitle.position.Set(Vsat.ScreenWidth2, creditsMusic.position.y + font.height * 2)
		creditsSpecialTitle.alignHorizontal = True
		creditsSpecialTitle.SetScale(0.8)
		
		creditsSpecial = New Label[3]
		For Local i:Int = 0 Until creditsSpecial.Length
			Select i
				Case 0
					creditsSpecial[0] = New Label("Max Gittel")
				Case 1
					creditsSpecial[1] = New Label("David Neubauer")
				Case 2
					creditsSpecial[2] = New Label("Sebastian Höhnl")
			End
			creditsSpecial[i].SetFont(font)
			creditsSpecial[i].color.Set(Color.White)
			creditsSpecial[i].position.Set(Vsat.ScreenWidth2, creditsSpecialTitle.position.y + font.height + (i * font.height*0.9))
			creditsSpecial[i].alignHorizontal = True
			creditsSpecial[i].SetScale(0.8)
		Next
		
		
	End
	
	Method InitLogo:Void()
		If IsHD()
			logo = New VSprite("fl_size3.png")
		Else
			logo = New VSprite("fl_size2.png")
		End
		logo.SetHandle(logo.Width/2, 0)
		logo.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.55 - logo.Height * 1.5)
		logo.color.Alpha = 0.0
	End
	

'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------
	Method OnUpdate:Void(dt:Float)
		mainMenuObject.UpdateParticles(dt)
		UpdateCursor()
		UpdateAlpha()
	End
	
	Method UpdateAlpha:Void()
		If Vsat.IsChangingScenes()
			language.color.Alpha = globalAlpha.Alpha * 0.9
			music.color.Alpha = globalAlpha.Alpha * 0.9
			musicOnOff.color.Alpha = globalAlpha.Alpha * 0.9
			creditsGameTitle.color.Alpha = globalAlpha.Alpha * 0.9
			logo.color.Alpha = globalAlpha.Alpha * 0.9
			creditsMusicTitle.color.Alpha = globalAlpha.Alpha * 0.9
			creditsMusic.color.Alpha = globalAlpha.Alpha * 0.9
			creditsSpecialTitle.color.Alpha = globalAlpha.Alpha * 0.9
			For Local i:Int = 0 Until creditsSpecial.Length
				creditsSpecial[i].color.Alpha = globalAlpha.Alpha * 0.9
			Next
		Else
			language.color.Alpha = 0.9
			music.color.Alpha = 0.9
			musicOnOff.color.Alpha = 0.9
			creditsGameTitle.color.Alpha = 0.9
			logo.color.Alpha = 0.9
			creditsMusicTitle.color.Alpha = 0.9
			creditsMusic.color.Alpha = 0.9
			creditsSpecialTitle.color.Alpha = 0.9
			For Local i:Int = 0 Until creditsSpecial.Length
				creditsSpecial[i].color.Alpha = 0.9
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
		RenderLanguage()
		RenderCredits()
		logo.Render()
		back.Render()
	End
	
	Method RenderBackground:Void()
		If Not Vsat.IsChangingScenes()
			Color.NewBlack.UseWithoutAlpha()
			SetAlpha(0.98)
			DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
			SetAlpha(0.05)
			Color.White.UseWithoutAlpha()
			DrawRect(0, 0, Vsat.ScreenWidth, language.position.y + (font.height * langs.Length * 1.5))
		End
	End
	
	Method RenderMusicOnOff:Void()
		music.Render()
		musicOnOff.Render()
	End
	
	Method RenderLanguage:Void()
		language.Render()
		For Local i:Int = 0 Until langs.Length
			langs[i].Render()
		Next
	End
	
	Method RenderCredits:Void()
		creditsGameTitle.Render()
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
			musicOnOff.isDown = False
			If musicOnOff.WasTouched(cursor)
				OnMusicOnOff()
			End
			Return
		End
		
		For Local i:Int = 0 Until langs.Length
			Local item:= langs[i]
			If item.isDown
				item.isDown = False
				If item.WasTouched(cursor)
					ChangeLanguage(item)
				End
			End
		Next
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
			Return
		End
		
		For Local i:Int = 0 Until langs.Length
			Local item:= langs[i]
			If item.WasTouched(cursor)
				item.isDown = True
			End
		Next
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
			music.Text = Localize.GetValue("settings_music_is_on")
			musicOnOff.Text = Localize.GetValue("settings_mute")
		Else
			Audio.Mute()
			music.Text = Localize.GetValue("settings_music_is_off")
			musicOnOff.Text = Localize.GetValue("settings_play")
		End
	End
	
	Method ChangeLanguage:Void(touchedItem:MenuItem)
		Select touchedItem.Text
			Case "English"
				If Localize.GetCurrentLanguage() <> "en"
					Localize.SetLanguage("en")
					Vsat.SaveToClipboard(mainMenuObject, "MainMenu")
					Vsat.ChangeScene(New SettingsScene)
					SaveGame()
				End
			Case "Deutsch"
				If Localize.GetCurrentLanguage() <> "de"
					Localize.SetLanguage("de")
					Vsat.SaveToClipboard(mainMenuObject, "MainMenu")
					Vsat.ChangeScene(New SettingsScene)
					SaveGame()
				End
		End
	End
	
	
	Private
	Field mainMenuObject:MainMenu
	Field lastTouchDown:Bool
	Field font:AngelFont
	Field lockedMenuItem:MenuItem
	
	Field music:Label
	Field musicOnOff:MenuItem
	
	Field language:Label
	Field langs:MenuItem[]
	
	Field creditsGameTitle:Label
	Field creditsMusicTitle:Label
	Field creditsMusic:Label
	Field creditsSpecialTitle:Label
	Field creditsSpecial:Label[]

	Field back:BackButton
	
	Field logo:VSprite

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


Class MenuItem Extends Label
	
	Field isDown:Bool
	Field downColor:Color = New Color(1.0, 1.0, 1.0, 0.8)
	
	Method New(withText:String)
		Super.New(withText)
		color.Set(Color.White)
	End
	
	Method Draw:Void()
		If isDown
			downColor.UseWithoutAlpha()
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




