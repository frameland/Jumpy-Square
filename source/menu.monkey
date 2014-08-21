Strict
Import vsat
Import enemy
Import game
Import extra
Import particles
Import particlebg
Import medalscene
Import buysupportermedal
Import supportermedal
Import flgamecenter
Import audio
Import settings
Import adventure


Class MainMenu Extends VScene Implements VActionEventHandler
	
	Field backgroundColor:Color = New Color($132b3b)
	Field backgroundColor2:Color = New Color($030026)
	Field currentBgColor:Color
	Field justGotSupporterMedal:Bool
	
	
'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------
	Method OnInit:Void()
		If initialized
			OnInitWhileInitialized()
			Return
		End
		
		InitAudio()
		LoadGame()
		
		currentBgColor = New Color(backgroundColor)
		
		font = FontCache.GetFont(RealPath("font"))
		font2 = FontCache.GetFont(RealPath("font2"))
		
		If Vsat.ScreenWidth = 768 'ipad
			scoreEnemyImage = LoadImage(RealPath("enemy_ipad.png"))
		Else
			scoreEnemyImage = LoadImage(RealPath("enemy.png"))
		End
		scoreEnemyImage.SetHandle(scoreEnemyImage.Width()/2, scoreEnemyImage.Height()/2)
		
		titleImage = LoadImage(RealPath("title.png"))
		titleImage.SetHandle(titleImage.Width()/2, 0)
		titleImage2 = LoadImage(RealPath("title2.png"))
		titleImage2.SetHandle(titleImage2.Width()/2, 0)
		titleTopSpacing = Vsat.ScreenHeight * 0.03
		lineHeight = font.TextHeight("Play") * 1.5
		highscoreSquareSize = Vsat.ScreenWidth * 0.22
		
		siteActive = ImageCache.GetImage(RealPath("siteActive.png"))
		siteNotActive = ImageCache.GetImage(RealPath("siteNotActive.png"))
		MidHandleImage(siteActive)
		MidHandleImage(siteNotActive)
		
		InitMenuItems()
		MenuIntroAnimation()
		
		backgroundEffect = New ParticleBackground
		
		InitMedalAndEffect()
		
		If IsHD()
			logo = LoadImage("fl_size2.png")
		Else
			logo = LoadImage("fl_size1.png")
		End
		
		Local transition:= New VFadeInLinear(1.0)
		transition.SetColor(Color.White)
		Vsat.StartFadeIn(transition)
		
		initialized = True
		
		
		
		Audio.Mute()
	End
	
	Method OnInitWhileInitialized:Void()
		InitMenuItems()
		For Local i:Int = 0 Until menuOptions.Length
			Local item:MenuItem = menuOptions[i]
			item.position.x = Vsat.ScreenWidth2 - item.size.x/2
			item.position.y = (lineHeight * i) + titleTopSpacing + highscoreSquareSize*2.5
		Next
		play2.position.Set(menuOptions[0].position)
		play2.position.x += Vsat.ScreenWidth
		InitMedalAndEffect()
	End
	
	Method InitMenuItems:Void()
		menuOptions = New MenuItem[3]
		menuOptions[0] = New MenuItem(Localize.GetValue("menu_play"), font)
		menuOptions[1] = New MenuItem(Localize.GetValue("menu_medals"), font)
		menuOptions[2] = New MenuItem(Localize.GetValue("menu_leaderboard"), font)
		
		settings = New MenuItem(Localize.GetValue("menu_settings"), font2)
		settings.SetIcon(RealPath("settings.png"))
		settings.position.x = Vsat.ScreenWidth * 0.95 - font2.TextWidth(settings.text)
		settings.position.y = Vsat.ScreenHeight - font2.height * 3
		settings.color.Alpha = 0.5
		
		'Site 2
		play2 = New MenuItem(Localize.GetValue("menu_play"), font)
		
	End
	
	Method MenuIntroAnimation:Void()
		For Local i:Int = 0 Until menuOptions.Length
			Local item:MenuItem = menuOptions[i]
			item.position.x = Vsat.ScreenWidth2 - item.size.x/2
			item.position.y = (lineHeight * i) + titleTopSpacing + highscoreSquareSize*2.5
			item.SetScale(1.5 + (1.0 - Float(i)/menuOptions.Length))
			Local scaleAction:= New VVec2ToAction(item.scale, 1.0, 1.0, 0.8, EASE_OUT_EXPO)
			Local delay:= New VDelayAction(0.2)
			AddAction(New VActionSequence([VAction(delay), VAction(scaleAction)]))
		Next
		
		play2.position.Set(menuOptions[0].position)
		play2.position.x += Vsat.ScreenWidth
	End
	
	Method InitMedalAndEffect:Void()
		If Not initialized
			supporterMedal = New SupporterMedal
			If GameScene.IsUnlocked
				supporterMedal.InitUnlocked()
			Else
				supporterMedal.InitLocked()
			End
		End
		
		supporterMedal.position.x = Vsat.ScreenWidth2
		Local lastMenuItem:= menuOptions[2]
		Local y:Float = (Vsat.ScreenHeight - lastMenuItem.position.y + lastMenuItem.usedFont.TextHeight(lastMenuItem.text)) / 2
		supporterMedal.position.y = y + lastMenuItem.position.y
		
		removeAdsText = New VLabel(Localize.GetValue("menu_remove_ads"))
		removeAdsText.SetFont(font2)
		removeAdsText.alignHorizontal = AngelFont.ALIGN_CENTER
		removeAdsText.position.Set(supporterMedal.position)
		removeAdsText.position.Add(0, supporterMedal.Height * 0.5)
		removeAdsText.SetScale(0.8)
		removeAdsText.color.Set(Color.Yellow)
		
		'Effect
		medalEffect = New ExplosionEmitter
		medalEffect.InitWithSize(40)
		medalEffect.particleLifeSpan = 1.0
		medalEffect.oneShot = True
		medalEffect.additiveBlend = True
		medalEffect.slowDownSpeed = 0.94
		
		medalEffect.position.Set(supporterMedal.position)
		medalEffect.speed = Vsat.ScreenWidth*0.22
		medalEffect.emissionAngleVariance = 180
		medalEffect.size.Set(Vsat.ScreenWidth*0.05, Vsat.ScreenWidth*0.05)
		
		medalEffect.startColor.Set(Color.Orange)
		medalEffect.endColor.Set(Color.Yellow)
		medalEffect.endColor.Alpha = 0.0
	End
	
	Method InitAudio:Void()
		Audio.PlayMusic("audio/music.mp3")
		Audio.SetMusicVolume(0.2)
		
		'Preload Sounds
		Audio.GetSound("audio/fadein.mp3")
		Audio.GetSound("audio/fadeout.mp3")
		Audio.GetSound("audio/explosion.mp3")
		Audio.GetSound("audio/grind.mp3")
		Audio.GetSound("audio/jump.mp3")
		Audio.GetSound("audio/wallhit.mp3")
		Audio.GetSound("audio/surprise.mp3")
		Audio.GetSound("audio/feed.mp3")
		Audio.GetSound("audio/double.mp3")
		Audio.GetSound("audio/new_highscore.mp3")
	End
	
	
	
	Method AddAction:Void(action:VAction)
		action.AddToList(actions)
		action.SetListener(Self)
	End
	
	
'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------
	Method OnUpdate:Void(dt:Float)
		VAction.UpdateList(actions, dt)
		UpdateParticles(dt)
		
		If startedConnecting
			If GameCenterIsConnecting()
				Return
			Else
				ShowGameCenter()
				startedConnecting = False
			End
		End
		
		UpdateSites()
		UpdateCursor()
		supporterMedal.Update(dt)
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
	
	Method UpdateSites:Void()
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
	
	Method UpdateParticles:Void(dt:Float)
		backgroundEffect.Update(dt)
		medalEffect.Update(dt)
	End
	

'--------------------------------------------------------------------------
' * Render
'--------------------------------------------------------------------------
	Method OnRender:Void()
		#rem
		If Vsat.transition And VFadeInLinear(Vsat.transition)
			Local scale:Float = Vsat.transition.Progress
			If scale < 0.01 scale = 0.0
			scale = 1.5 - Tweening(EASE_OUT_EXPO, scale, 0.0, 1.0, 0.8) * 0.5
			ScaleAt(Vsat.ScreenWidth2, Vsat.ScreenHeight2, scale, scale)
		End
		#end
		
		If Vsat.IsActiveScene(Self) Then RenderParticles()
		PushMatrix()
			Translate(-currentPosX, 0)
			RenderHighscore()
			RenderTitle()
			RenderMenu()
			RenderSupporterMedal()
			RenderLogo()
			RenderSite2()
		PopMatrix()
		
		RenderSites()
		RenderConnectingToGameCenter()
	End
	
	Method RenderParticles:Void()
		backgroundEffect.Render()
	End
	
	Method RenderTitle:Void()
		ResetBlend()
		Color.White.Use()
		SetAlpha(1.0)
		DrawImage(titleImage, Vsat.ScreenWidth2, titleTopSpacing)
		DrawImage(titleImage2, Vsat.ScreenWidth*1.5, titleTopSpacing)
	End
	
	Method RenderHighscore:Void()
		ResetBlend()
		PushMatrix()
			Color.White.Use()
			
			Translate(Vsat.ScreenWidth2, titleTopSpacing + highscoreSquareSize * 1.8)
			PushMatrix()
				Scale(0.8, 0.8)
				Rotate(-Vsat.Seconds*45)
				DrawImage(scoreEnemyImage, 0, 0, 0, 1.4, 1.4)
			PopMatrix()
			
			PushMatrix()
				Translate(Vsat.ScreenWidth, 0)
				Scale(0.8, 0.8)
				Rotate(-Vsat.Seconds*45)
				DrawImage(scoreEnemyImage, 0, 0, 0, 1.4, 1.4)
			PopMatrix()
			
			Color.Orange.Use()
			font.DrawText(GameScene.Highscore, 0, -5, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
			font.DrawText(GameScene.HighscoreAdventure, Vsat.ScreenWidth, -5, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
		PopMatrix()
	End
	
	Method RenderMenu:Void()
		ResetBlend()
		For Local i:Int = 0 Until menuOptions.Length
			menuOptions[i].Render()
		Next
		settings.Render()
		play2.Render()
	End
	
	Method RenderSupporterMedal:Void()
		If GameScene.IsUnlocked
			Local fadeInTime:Float = 1.0
			If supporterMedal.UnlockTime > Vsat.Seconds - fadeInTime
				supporterMedal.color.Alpha = (Vsat.Seconds - supporterMedal.UnlockTime) * 1.0/fadeInTime
			Else
				supporterMedal.color.Alpha = 1.0
			End
		Else
			Local alpha:Float = Min(0.8 + Sin(Vsat.Seconds*150)*0.5, 1.0)
			supporterMedal.color.Alpha = alpha
		End
		removeAdsText.color.Alpha = supporterMedal.color.Alpha - 0.2
		
		supporterMedal.Render()
		If GameScene.IsUnlocked = False
			removeAdsText.Render()
		End
		
		If justGotSupporterMedal
			justGotSupporterMedal = False
			medalEffect.Start()
			supporterMedal.InitUnlocked()
		End
		medalEffect.Render()
	End
	
	Method RenderLogo:Void()
		SetAlpha(0.3)
		Color.White.UseWithoutAlpha()
		PushMatrix()
			Translate(Vsat.ScreenWidth * 0.05, settings.position.y + 2)
			'Scale(0.8, 0.8)
			DrawImage(logo, 0, 0)
		PopMatrix()
	End
	
	Method RenderSites:Void()
		ResetColor()
		SetAlpha(0.8)
		DrawRect(0, Int(Vsat.ScreenHeight * 0.96), Vsat.ScreenWidth, 1)
		
		ResetColor()
		Local siteRadius:Float = siteActive.Width()/2 * 0.8
		Local x:Float = Vsat.ScreenWidth2 + siteRadius
		If sites Mod 2 = 0
			x -= (siteRadius * 3 * sites/2) / 2
		Else
			x -= (siteRadius * 3 * sites/2)
		End
		
		For Local i:Int = 1 To sites
			If i = currentSite
				DrawImage(siteActive, x, Vsat.ScreenHeight - siteRadius * 2)
			Else
				DrawImage(siteNotActive, x, Vsat.ScreenHeight - siteRadius * 2)
			End
			x += siteRadius * 3
		Next
		
	End
	
	Method RenderSite2:Void()
		ResetColor()
		PushMatrix()
			Translate(Vsat.ScreenWidth*1.5, Vsat.ScreenHeight - font.height * 5)
			font.DrawText("Level: " + GameScene.Level, 0, 0, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
			font.DrawText("Exp: " + GameScene.Exp, 0, font.height, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
			font.DrawText("Gold: " + GameScene.Gold, 0, font.height*2, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
		PopMatrix()
	End
	
	Method RenderConnectingToGameCenter:Void()
		If GameCenterIsConnecting()
			Local dots:Int = Int(Vsat.Seconds * 1000) Mod 1600
			
			SetAlpha(0.9)
			Color.Black.UseWithoutAlpha()
			DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
			
			Local connectText:String = Localize.GetValue("connecting")
			Local xPos:Int = Vsat.ScreenWidth2 - font.TextWidth(connectText)/2
			If dots > 1200
				connectText += "..."
			ElseIf dots > 800
				connectText += ".."
			ElseIf dots > 400
				connectText += "."
			End
			
			Color.White.UseWithoutAlpha()
			SetAlpha(0.5)
			font.DrawText(connectText, xPos, Vsat.ScreenHeight2, AngelFont.ALIGN_LEFT, AngelFont.ALIGN_CENTER)
		End
	End
	
	Method ClearScreen:Void()
		ClearScreenWithColor(currentBgColor)
	End
	
	
'--------------------------------------------------------------------------
' * Events
'--------------------------------------------------------------------------
	Method OnActionEvent:Void(id:Int, action:VAction)
		If id = VAction.FINISHED
		End
	End
	
	Method OnMouseUp:Void()
		For Local i:Int = 0 Until menuOptions.Length
			menuOptions[i].isDown = False
		Next
		lockedMenuItem = Null
		settings.isDown = False
		play2.isDown = False
		
		If PointInRect(MouseX() + (Vsat.ScreenWidth * (currentSite-1)), MouseY(), Vsat.ScreenWidth * 0.05, settings.position.y + 2, logo.Width(), logo.Height())
			OpenUrl("http://frameland.at")
		End
		
		'Swiping
		touchEndX = TouchX() + (Vsat.ScreenWidth * (currentSite-1))
		If touchEndX = touchStartX
			CheckMenuClicked(True)
			CheckMedalClicked()
			Return
		End
		touchTime = Vsat.Seconds - touchStartTime
		Local distance:Float = touchEndX - touchStartX
		Local speed:Float = distance / touchTime
		If Abs(speed) > Vsat.ScreenWidth2
			Local previousSite:Int = currentSite
			If speed < 0
				currentSite += 1
				currentSite = Min(currentSite, sites)
			ElseIf speed > 0
				currentSite -= 1
				currentSite = Max(currentSite, 1)
			End
			If currentSite <> previousSite
				OnSiteChange()
			End
			
		End
		targetPosX = Vsat.ScreenWidth * (currentSite-1)
	End
	
	Method OnMouseDown:Void()
		If touchStartX = TouchX() Then CheckMenuClicked(False)
		
		If lastTouchDown = False
			touchStartX = TouchX() + (Vsat.ScreenWidth * (currentSite-1))
			touchStartTime = Vsat.Seconds
		Else
			currentPosX = touchStartX - TouchX()
		End
	End
	
	Method CheckMenuClicked:Void(up:Bool = True)
		Local cursor:Vec2 = New Vec2(TouchX() + (Vsat.ScreenWidth * (currentSite-1)), TouchY())
		
		If currentSite = 2
			If play2.WasTouched(cursor)
				If up
					OnMenuClicked(play2)
				ElseIf lockedMenuItem = Null
					play2.isDown = True
					lockedMenuItem = play2
				End
				Return
			End
		End
		
		If settings.WasTouched(cursor)
			If up
				OnMenuClicked(settings)
			ElseIf lockedMenuItem = Null
				settings.isDown = True
				lockedMenuItem = settings
			End
			Return
		End
		
		For Local i:Int = 0 Until menuOptions.Length
			Local item:= menuOptions[i]
			If item.WasTouched(cursor)
				If up
					OnMenuClicked(item)
					Return
				ElseIf lockedMenuItem = Null
					item.isDown = True
					lockedMenuItem = item
				End
			End
		Next
		
	End
	
	Method OnMenuClicked:Void(item:MenuItem)
		Select item.text
			Case menuOptions[0].text, play2.text
				If currentSite = 1
					PlayClassic()
				ElseIf currentSite = 2
					PlayAdventure()
				End
			Case menuOptions[1].text
				GoToMedals()
			Case menuOptions[2].text
				OpenLeaderboard()
			Case settings.text
				GoToSettings()
		End
	End
	
	Method CheckMedalClicked:Void()
		Local tx:Float = TouchX()
		Local ty:Float = TouchY()
		Local w:Int = supporterMedal.Width()
		Local h:Int = supporterMedal.Height()
		If PointInRect(tx, ty, Vsat.ScreenWidth2 - w/2, supporterMedal.position.y - h/2, w, h) And currentSite = 1
			GoToSupporter()
		End
	End
	
	Method OnSiteChange:Void()
		If currentSite = 1
			Local color:= New VFadeToColorAction(currentBgColor, backgroundColor, 0.5, LINEAR_TWEEN)
			AddAction(color)
		ElseIf currentSite = 2
			Local color:= New VFadeToColorAction(currentBgColor, backgroundColor2, 0.5, LINEAR_TWEEN)
			AddAction(color)
		End
	End
	

'--------------------------------------------------------------------------
' * Menu Events
'--------------------------------------------------------------------------	
	Method PlayClassic:Void()
		If Vsat.transition And VFadeInLinear(Vsat.transition) = Null
			Return
		End
		
		Vsat.SaveToClipboard(Self, "MainMenu")
		Vsat.SaveToClipboard(Self.backgroundEffect, "BgEffect")
		
		Local game:GameScene = New GameScene
		game.InitAds()
		game.HideAds()
		backgroundEffect.SetPlay()
		Vsat.ChangeScene(game)
		
		Audio.PlaySound(Audio.GetSound("audio/fadein.mp3"), 1)
	End
	
	Method PlayAdventure:Void()
		If Vsat.transition And VFadeInLinear(Vsat.transition) = Null
			Return
		End
		
		Vsat.SaveToClipboard(Self, "MainMenu")
		Vsat.SaveToClipboard(Self.backgroundEffect, "BgEffect")
		
		Local scene:= New AdventureScene
		scene.InitAds()
		scene.HideAds()
		backgroundEffect.SetPlay()
		Vsat.ChangeScene(scene)
		
		Audio.PlaySound(Audio.GetSound("audio/fadein.mp3"), 1)
	End
	
	Method GoToMedals:Void()
		If Vsat.transition And VFadeInLinear(Vsat.transition) = Null
			Return
		End
		
		Vsat.SaveToClipboard(Self, "MainMenu")
		Vsat.SaveToClipboard(Self.backgroundEffect, "BgEffect")
		Self.shouldClearScreen = False
		Local medals:= New MedalScene
		Vsat.ChangeScene(medals)
		
		Audio.PlaySound(Audio.GetSound("audio/fadein.mp3"), 1)
	End
	
	Method GoToSupporter:Void()
		If Vsat.transition And VFadeInLinear(Vsat.transition) = Null
			Return
		End
		
		Vsat.SaveToClipboard(Self, "MainMenu")
		Local scene:= New BuySupporterMedalScene
		Vsat.ChangeScene(scene)
		
		Audio.PlaySound(Audio.GetSound("audio/fadein.mp3"), 1)
	End
	
	Method GoToSettings:Void()
		If Vsat.transition And VFadeInLinear(Vsat.transition) = Null
			Return
		End
		
		Vsat.SaveToClipboard(Self, "MainMenu")
		Local scene:= New SettingsScene
		Vsat.ChangeScene(scene)
		
		Audio.PlaySound(Audio.GetSound("audio/fadein.mp3"), 1)
	End
	
	Method OpenLeaderboard:Void()
		If Vsat.transition And VFadeInLinear(Vsat.transition) = Null
			Return
		End
		InitGameCenter()
		SyncGameCenter(GameScene.Highscore)
		startedConnecting = True
	End

	
'--------------------------------------------------------------------------
' * Private
'--------------------------------------------------------------------------
	Field initialized:Bool
	
	Field font:AngelFont
	Field font2:AngelFont
	Field titleImage:Image, titleImage2:Image
	Field scoreEnemyImage:Image
	
	Field menuOptions:MenuItem[]
	Field settings:MenuItem
	Field lockedMenuItem:MenuItem
	
	Field enemies:List<Enemy> = New List<Enemy>
	Field enemyTimer:Float = 1.0
	Field lastSpawnedLeft:Bool = True
	Field titleTopSpacing:Int
	Field highscoreSquareSize:Int
	Field lineHeight:Int
	
	Field lastTouchDown:Bool
	
	Field actions:List<VAction> = New List<VAction>
	
	Field backgroundEffect:ParticleBackground
	
	Field medalEffect:ExplosionEmitter
	Field supporterMedal:SupporterMedal
	Field removeAdsText:VLabel
	
	Field startedConnecting:Bool
	
	Field logo:Image
	Field siteActive:Image
	Field siteNotActive:Image
	
	Field sites:Int = 2
	Field currentSite:Int = 1
	Field currentPosX:Float
	Field targetPosX:Float = -99999
	Field touchStartX:Float
	Field touchEndX:Float
	Field touchStartTime:Float
	Field touchTime:Float
	
	'Site 2
	Field play2:MenuItem
End




Private
Class MenuItem Extends VRect
	
	Field text:String
	Field usedFont:AngelFont
	Field isDown:Bool
	Field icon:Image
	
	Method New(withText:String, withFont:AngelFont)
		Super.New(0, 0, 100, 20)
		color.Set(Color.White)
		usedFont = withFont
		text = withText
		size.x = usedFont.TextWidth(text)
		size.y = usedFont.TextHeight(text)
	End
	
	Method SetIcon:Void(path:String)
		icon = LoadImage(path)
		MidHandleImage(icon)
	End
	
	Method Draw:Void()
		If isDown
			Color.Orange.Use()
		End
		usedFont.DrawText(text, 0.5, 0, AngelFont.ALIGN_LEFT, AngelFont.ALIGN_TOP)
		If icon
			DrawImage(icon, -icon.Width()*0.6, icon.Height()/2)
		End
	End
	
	Method Render:Void()
		color.Use()
		PushMatrix()
			Translate(position.x, position.y)
			Rotate(rotation)
			Scale(scale.x, scale.y)
			Draw()
		PopMatrix()
	End
	
	Method Text:Void(text:String) Property
		Self.text = text
		size.x = usedFont.TextWidth(text)
		size.y = usedFont.TextHeight(text)
	End
	
	Method WasTouched:Bool(cursor:Vec2)
		If color.Alpha <= 0.001 Return False
		Local touchsizeBufferX:Float = size.x * 0.2
		Local touchsizeBufferY:Float = size.y * 0.2
		If icon
			Return PointInRect(cursor.x, cursor.y, position.x-touchsizeBufferX-icon.Width(), position.y-touchsizeBufferY, size.x+touchsizeBufferX*2+icon.Width(), size.y+touchsizeBufferY*2)
		End
		Return PointInRect(cursor.x, cursor.y, position.x-touchsizeBufferX, position.y-touchsizeBufferY, size.x+touchsizeBufferX*2, size.y+touchsizeBufferY*2)
	End
	
End




