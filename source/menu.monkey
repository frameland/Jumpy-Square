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


Class MainMenu Extends VScene Implements VActionEventHandler
	
	Field backgroundColor:Color = New Color($132b3b)
	Field justGotSupporterMedal:Bool
	
	
'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------
	Method OnInit:Void()
		If initialized
			OnInitWhileInitialized()
			Return
		End
		initialized = True
		
		LoadGame()
		
		font = FontCache.GetFont(RealPath("font"))
		
		scoreEnemyImage = LoadImage(RealPath("enemy.png"))
		scoreEnemyImage.SetHandle(scoreEnemyImage.Width()/2, scoreEnemyImage.Height()/2)
		
		titleImage = LoadImage(RealPath("title.png"))
		titleImage.SetHandle(titleImage.Width()/2, 0)
		titleTopSpacing = Vsat.ScreenHeight * 0.1
		lineHeight = font.TextHeight("Play") * 1.5
		highscoreSquareSize = Vsat.ScreenWidth * 0.22
		
		menuOptions = New MenuItem[3]
		menuOptions[0] = New MenuItem("Play", font)
		menuOptions[1] = New MenuItem("Medals", font)
		menuOptions[2] = New MenuItem("GameCenter", font)
		MenuIntroAnimation()
		
		backgroundEffect = New ParticleBackground
		
		InitMedalAndEffect()
		
		Local transition:= New VFadeInLinear(1.2)
		transition.SetColor(Color.White)
		Vsat.StartFadeIn(transition)
	End
	
	Method OnInitWhileInitialized:Void()
		
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
	End
	
	Method InitMedalAndEffect:Void()
		supporterMedal = New SupporterMedal
		If GameScene.IsUnlocked
			supporterMedal.InitUnlocked()
		Else
			supporterMedal.InitLocked()
		End
		supporterMedal.position.x = Vsat.ScreenWidth2
		Local lastMenuItem:= menuOptions[2]
		Local y:Float = (Vsat.ScreenHeight - lastMenuItem.position.y + lastMenuItem.usedFont.TextHeight(lastMenuItem.text)) / 2
		supporterMedal.position.y = y + lastMenuItem.position.y
		
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
	
	Method AddAction:Void(action:VAction)
		action.AddToList(actions)
		action.SetListener(Self)
	End
	
	
'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------
	Method OnUpdate:Void(dt:Float)
		VAction.UpdateList(actions, dt)
		UpdateCursor()
		UpdateParticles(dt)
		supporterMedal.Update(dt)
	End
	
	Method UpdateCursor:Void()
		If TouchDown()
			lastTouchDown = True
			OnMouseDown()
		Else
			If lastTouchDown
				OnMouseUp()
			End
			lastTouchDown = False
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
		If Vsat.transition And VFadeInLinear(Vsat.transition)
			Local scale:Float = Vsat.transition.Progress
			If scale < 0.01 scale = 0.0
			scale = 1.5 - Tweening(EASE_OUT_EXPO, scale, 0.0, 1.0, 0.8) * 0.5
			ScaleAt(Vsat.ScreenWidth2, Vsat.ScreenHeight2, scale, scale)
		End
		
		If Vsat.IsActiveScene(Self) Then RenderParticles()
		RenderHighscore()
		RenderTitle()
		RenderMenu()
		RenderSupporterMedal()
	End
	
	Method RenderParticles:Void()
		backgroundEffect.Render()
	End
	
	Method RenderTitle:Void()
		ResetBlend()
		Color.White.Use()
		SetAlpha(1.0)
		DrawImage(titleImage, Vsat.ScreenWidth2, titleTopSpacing)
	End
	
	Method RenderHighscore:Void()
		ResetBlend()
		PushMatrix()
			Translate(Vsat.ScreenWidth2, titleTopSpacing + highscoreSquareSize * 1.5)
			Color.White.Use()
			PushMatrix()
				Rotate(-Vsat.Seconds*45)
				DrawImage(scoreEnemyImage, 0, 0, 0, 1.4, 1.4)
			PopMatrix()
			Color.Orange.Use()
			font.DrawText(GameScene.Highscore, 0, -5, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
		PopMatrix()
	End
	
	Method RenderMenu:Void()
		ResetBlend()
		For Local i:Int = 0 Until menuOptions.Length
			menuOptions[i].Render()
		Next
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
			Local alpha:Float = Min(0.7 + Sin(Vsat.Seconds*150)*0.5, 1.0)
			supporterMedal.color.Alpha = alpha
		End
		supporterMedal.Render()
		
		If justGotSupporterMedal
			justGotSupporterMedal = False
			medalEffect.Start()
			supporterMedal.InitUnlocked()
		End
		medalEffect.Render()
	End
	
	Method ClearScreen:Void()
		ClearScreenWithColor(backgroundColor)
	End
	
	
'--------------------------------------------------------------------------
' * Events
'--------------------------------------------------------------------------
	Method OnActionEvent:Void(id:Int, action:VAction)
		If id = VAction.FINISHED
			
		End
	End
	
	Method OnMouseUp:Void()
		CheckMedalClicked()
		CheckMenuClicked()
		For Local i:Int = 0 Until menuOptions.Length
			menuOptions[i].isDown = False
		Next
		lockedMenuItem = Null
	End
	
	Method OnMouseDown:Void()
		CheckMenuClicked(False)
	End
	
	Method CheckMenuClicked:Void(up:Bool = True)
		Local cursor:Vec2 = New Vec2(TouchX(), TouchY())
		For Local i:Int = 0 Until menuOptions.Length
			Local item:= menuOptions[i]
			If item.WasTouched(cursor)
				If up
					OnMenuClicked(item)
				ElseIf lockedMenuItem = Null
					item.isDown = True
					lockedMenuItem = item
				End
			End
		Next
	End
	
	Method OnMenuClicked:Void(item:MenuItem)
		Select item.text
			Case "Play"
				GoToGame()
			Case "Medals"
				GoToMedals()
			Case "GameCenter"
				
		End
	End
	
	Method CheckMedalClicked:Void()
		Local tx:Float = TouchX()
		Local ty:Float = TouchY()
		Local w:Int = supporterMedal.Width()
		Local h:Int = supporterMedal.Height()
		If PointInRect(tx, ty, Vsat.ScreenWidth2 - w/2, supporterMedal.position.y - h/2, w, h)
			GoToSupporter()
		End
	End
	
	Method GoToGame:Void()
		If Vsat.transition And VFadeInLinear(Vsat.transition) = Null
			Return
		End
		
		Vsat.SaveToClipboard(Self, "MainMenu")
		Vsat.SaveToClipboard(Self.backgroundEffect, "BgEffect")
		Self.shouldClearScreen = False
		Local game:GameScene = New GameScene
		Vsat.ChangeScene(game)
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
	End
	
	Method GoToSupporter:Void()
		If Vsat.transition And VFadeInLinear(Vsat.transition) = Null
			Return
		End
		Vsat.SaveToClipboard(Self, "MainMenu")
		Local scene:= New BuySupporterMedalScene
		Vsat.ChangeScene(scene)
	End
	
	
'--------------------------------------------------------------------------
' * Private
'--------------------------------------------------------------------------
	Private
	Field initialized:Bool
	
	Field font:AngelFont
	Field titleImage:Image
	Field scoreEnemyImage:Image
	
	Field menuOptions:MenuItem[]
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
	
End




Private
Class MenuItem Extends VRect
	
	Field text:String
	Field usedFont:AngelFont
	Field isDown:Bool
	
	Method New(withText:String, withFont:AngelFont)
		Super.New(0, 0, 100, 20)
		color.Set(Color.White)
		usedFont = withFont
		text = withText
		size.x = usedFont.TextWidth(text)
		size.y = usedFont.TextHeight(text)
	End
	
	Method Draw:Void()
		If isDown
			Color.Orange.Use()
		End
		usedFont.DrawText(text, 0.5, 0, AngelFont.ALIGN_LEFT, AngelFont.ALIGN_TOP)
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
	
	Method WasTouched:Bool(cursor:Vec2)
		Local touchsizeBufferX:Float = size.x * 0.2
		Local touchsizeBufferY:Float = size.y * 0.2
		Return PointInRect(cursor.x, cursor.y, position.x-touchsizeBufferX, position.y-touchsizeBufferY, size.x+touchsizeBufferX*2, size.y+touchsizeBufferY*2)
	End
	
End




