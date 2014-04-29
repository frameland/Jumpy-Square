Strict
Import vsat
Import enemy
Import game
Import extra
Import particles
Import particlebg
Import medalscene

Class MainMenu Extends VScene Implements VActionEventHandler
	
	Const TITLE:String = "Jumpy Square"
	Field backgroundColor:Color = New Color($132b3b)
	
	
'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------
	Method OnInit:Void()
		If initialized
			IntroAnimationWhenAlreadyInitialized()
			Return
		End
		initialized = True
		
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
		
		Local transition:= New VFadeInLinear(1.2)
		transition.SetColor(Color.White)
		Vsat.StartFadeIn(transition)
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
	
	Method IntroAnimationWhenAlreadyInitialized:Void()
		
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
		'UpdateEnemySpawning(dt)
		'UpdateEnemies(dt)
		UpdateParticles(dt)
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
	
	Method UpdateEnemySpawning:Void(dt:Float)
		If Vsat.IsChangingScenes Return
		
		enemyTimer -= dt
		If enemyTimer <= 0.0
			enemyTimer = 0.8 + Rnd() * 2
			Local e:= New Enemy
			e.color.Set(Color.Gray)
			e.accountsForPoints = False
			e.gravity /= 2
			If lastSpawnedLeft
				If Rnd() > 0.7
					e.SetLeft()
					lastSpawnedLeft = True
				Else
					e.SetRight()
					lastSpawnedLeft = False
				End
			Else
				If Rnd() > 0.7
					e.SetRight()
					lastSpawnedLeft = False
				Else
					e.SetLeft()
					lastSpawnedLeft = True
				End
			End
			e.link = enemies.AddLast(e)
		End
	End
	
	Method UpdateEnemies:Void(dt:Float)
		For Local e:= EachIn enemies
			e.UpdatePhysics(dt)
		Next
	End
	
	Method UpdateParticles:Void(dt:Float)
		backgroundEffect.Update(dt)
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
		
		If Vsat.IsActiveScene(Self) Then backgroundEffect.Render()
		'RenderEnemies()
		RenderHighscore()
		RenderTitle()
		RenderMenu()
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
	
	Method RenderEnemies:Void()
		For Local e:= EachIn enemies
			e.Render()
		Next
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




