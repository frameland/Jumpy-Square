Strict
Import vsat
Import enemy
Import game
Import extra
Import particles
Import particlebg


Class MainMenu Extends VScene Implements VActionEventHandler
	
	Const TITLE:String = "Jumpy Square"
	
'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------
	Method OnInit:Void()
		font = New AngelFont
		font.LoadFromXml("lane_narrow")
		fontBig = New AngelFont
		fontBig.LoadFromXml("lane_narrow_big")
		
		Print fontBig.TextHeight("0")
		
		titleTopSpacing = Vsat.ScreenHeight * 0.1
		lineHeight = font.TextHeight("Play") * 1.5
		
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
			item.position.y = (Vsat.ScreenHeight - lineHeight * menuOptions.Length) * 0.75 + (lineHeight * i)
			item.SetScale(1.5 + (1.0 - Float(i)/menuOptions.Length))
			Local scaleAction:= New VVec2ToAction(item.scale, 1.0, 1.0, 0.8, EASE_OUT_EXPO)
			Local delay:= New VDelayAction(0.2)
			AddAction(New VActionSequence([VAction(delay), VAction(scaleAction)]))
		Next
		
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
		UpdateEnemySpawning(dt)
		UpdateEnemies(dt)
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
		RenderEnemies()
		RenderHighscore()
		RenderTitle()
		RenderMenu()
	End
	
	Method RenderTitle:Void()
		ResetBlend()
		Color.NewBlack.Use()
		SetAlpha(0.7)
		fontBig.DrawText(TITLE, Vsat.ScreenWidth2, titleTopSpacing, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
	End
	
	Method RenderHighscore:Void()
		ResetBlend()
		Color.NewRed.Use()
		SetAlpha(0.8)
		
		PushMatrix()
			Translate(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.4)
			Rotate(Sin(Vsat.Seconds * 100) * 5 + 45)
			Local scale:Float = 0.8 + Abs(Sin(Vsat.Seconds * 100) * 0.1)
			Scale(scale, scale)
			Local pos:Float = Vsat.ScreenWidth * 0.14
			DrawRectOutline(-pos, -pos, pos * 2, pos * 2)
			DrawRectOutline(-pos * 0.8, -pos * 0.8, pos * 1.6, pos * 1.6)
			pos = Vsat.ScreenWidth * 0.17
			' DrawCircleOutline(0, 0, pos)
			Rotate(-45)
			'DrawRectOutline(-pos, -pos, pos * 2, pos * 2)
			fontBig.DrawText(GameScene.Highscore, 0, -15, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
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
		ClearScreenWithColor(Color.Silver)
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
			Case "Stats"
				GoToStats()
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
	
	Method GoToStats:Void()
		
	End
	
	
'--------------------------------------------------------------------------
' * Private
'--------------------------------------------------------------------------
	Private
	Field font:AngelFont
	Field fontBig:AngelFont
	
	Field menuOptions:MenuItem[]
	Field lockedMenuItem:MenuItem
	
	Field enemies:List<Enemy> = New List<Enemy>
	Field enemyTimer:Float = 1.0
	Field lastSpawnedLeft:Bool = True
	Field titleTopSpacing:Int
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
		color.Set(Color.Gray)
		usedFont = withFont
		text = withText
		size.x = usedFont.TextWidth(text)
		size.y = usedFont.TextHeight(text)
	End
	
	Method Draw:Void()
		If isDown
			Color.NewBlue.Use()
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




