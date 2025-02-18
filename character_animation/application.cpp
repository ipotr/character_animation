#include "application.h"

#include "d3d_window.h"
#include "d3d_manager.h"

#include "pascal.h"

_vec3         application::_up_axis        = _vec3(0.0f,1.0f,0.0f);
application*  application::_instance       = NULL;
HINSTANCE     application::_win32_instance = NULL;

application::application(){

	m_font         = NULL;
	m_pascal       = NULL;

	m_look         = _vec3(0.0f,-1.0f,0.0f);
	m_target       = _vec3(0.0f,0.0f,0.0f);
	m_position     = _vec3(0.0f,30.0f,0.0f);

	m_y_pos        = 0.0f;
	m_x_pos        = 0.0f;

	m_yaw          = -45.0f;
	m_pitch        = -10.0f;
	m_yaw_pos      = -45.0f;
	m_pitch_pos    = -10.0f;
	m_distance     = 10.0f;

	m_x_cursor_pos = 0.0f;
	m_y_cursor_pos = 0.0f;

	m_keyframe = 0;
	m_framespersecond = 0;
	m_last_frame_seconds = 0;
	m_last_frame_milliseconds = 0;

}

bool application::init(){

	/*win32 HINSTANCE */
	_win32_instance = GetModuleHandle(NULL);

	_window         = new d3d_window();
	_api_manager    = new d3d_manager();

	if(!_window->init())      { return false;}
	if(!_api_manager->init()) { return false; }

	/* when this flag is removed, the application exits */
	addflags(application_running); 

	/*skeletan animation object (character)*/
	m_pascal = new pascal_object();
	if(!m_pascal->init()) { return false; }
	m_pascal->setbindpose();
	/***************************************/

	/*stat display font****************************************/
	D3DXFONT_DESC fontDesc;
	fontDesc.Height          = 10;
	fontDesc.Width           = 5;
	fontDesc.Weight          = FW_SEMIBOLD;
	fontDesc.MipLevels       = 0;
	fontDesc.CharSet         = DEFAULT_CHARSET;
	fontDesc.OutputPrecision = OUT_DEFAULT_PRECIS;
	fontDesc.Quality         = DEFAULT_QUALITY;
	fontDesc.PitchAndFamily  = DEFAULT_PITCH | FF_DONTCARE;
	/*crude solution to avoid compiler warning*****************/
	application_zero(fontDesc.FaceName,20);
	const char *fontname = "Times New Roman";
	for(int i=0;i<16;i++){ fontDesc.FaceName[i] = fontname[i]; }
	application_throw_hr(D3DXCreateFontIndirect(_api_manager->m_d3ddevice, &fontDesc, &m_font));
	/**********************************************************/

	return true;
}

void application::run(){

	onresetdevice();

	m_keyframe =1; // walk animation
	_application->m_pascal->keyframe(0,5);
	_application->m_pascal->m_animation_length =0.14f;

	_window->update();

	/* initialize  timer */
	int64_t tickspersecond = 0;
	int64_t previous_timestamp = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&tickspersecond);
	float secsPerCnt = 1.0f / (float)tickspersecond;
	QueryPerformanceCounter((LARGE_INTEGER*)&previous_timestamp);
	/********************************/

	onresetdevice();

	int frames = 0;
	float second =0.0f;
	while( testflags(application_running) ){

		/**d3d device test.  error exits application**/
		removeflags(application_lostdev);
		HRESULT hr = _api_manager->m_d3ddevice->TestCooperativeLevel();

		if( hr == D3DERR_DEVICELOST ) {  addflags( application_lostdev );  }
		else if( hr == D3DERR_DRIVERINTERNALERROR ) {
			addflags( application_deverror );
			application_error("d3d device error");
		}else if( hr == D3DERR_DEVICENOTRESET ){
			_api_manager->reset();
			addflags( application_lostdev );
		}
		/*********************************************/

		/*timer****************************************************/
		int64_t currenttimestamp = 0;
		QueryPerformanceCounter((LARGE_INTEGER*)&currenttimestamp);
		m_last_frame_seconds = float(currenttimestamp - previous_timestamp) * secsPerCnt;
		m_last_frame_milliseconds = m_last_frame_seconds*1000.0f;
		previous_timestamp = currenttimestamp;
		/**********************************************************/

		frames++;
		second += m_last_frame_seconds;
		/*stat string generation **************************************************/
		static _string stats;
		if(second >=1.0f){
			stats = _string("mspf: ")+_utility::floattostring(m_last_frame_milliseconds);
			stats = stats + _string(" fps: ");
			stats = stats + _utility::inttostring(frames);
			second = 0.0f;
			frames = 0;
		}
		/**************************************************************************/

		if( !(testflags(application_lostdev))  && !(testflags(application_deverror)) &&!(testflags(application_paused))  ) {
			/* application update (render) ******************************************/

			update();

			application_error_hr(_api_manager->m_d3ddevice->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(250, 250, 250), 1.0f, 0));
			application_error_hr(_api_manager->m_d3ddevice->BeginScene());

			RECT rect;
			application_zero(&rect,sizeof(RECT));
			rect.right  = stats.m_count*5+40;
			rect.bottom = 20;
			m_font->DrawText(0,!stats.m_data?" ":stats.m_data, -1,&rect, DT_CENTER | DT_VCENTER, D3DCOLOR_XRGB(55, 55, 55));

			m_pascal->update();

			application_error_hr(_api_manager->m_d3ddevice->EndScene());
			application_error_hr(_api_manager->m_d3ddevice->Present(0, 0, 0, 0));
			/************************************************************************/
		}

		if( testflags(application_deverror) ) { removeflags(application_running); }
		else { _window->update(); }/* winpoc (input) */
	}

	/* deallocate .. exiting */
	m_pascal->clear();
	clear();
	/**********************/
}

void application::clear(){
	application_releasecom(m_font);
	if(_api_manager ) { _api_manager->clear();}
}

bool application::update(){

	/** spherical camera*********************************/
	m_pitch = (m_pitch >  45.0f ) ? 45.0f  : m_pitch;
	m_yaw   = (m_yaw   >  180.0f) ? 180.0f : m_yaw;
	m_pitch = (m_pitch < -90.0f ) ?-90.0f  : m_pitch;
	m_yaw   = (m_yaw   < -180.0f) ?-180.0f : m_yaw;

	_mat4  r = _yawpitchroll(float(_radians(m_yaw)),float(_radians(m_pitch)),0.0f);
	_vec4 t  = r*_vec4(0,0,m_distance,0.0f);
	_vec4 up = r*_vec4(_up_axis.x,_up_axis.y,_up_axis.z,0.0f);

	m_up = _vec3(up.x,up.y,up.z);
	m_position = m_target + _vec3(t.x,t.y,t.z);
	m_look = _normalize(m_target-m_position);

	m_view = _lookatrh(m_position, m_target, m_up);
	//***************************************************/

	//*cursor update*************************************/
	POINT cursor_position;
	if (GetCursorPos(&cursor_position)) {
		m_x_cursor_pos = float(cursor_position.x);
		m_y_cursor_pos = float(cursor_position.y);
	}else{ application_error("cursor pos");}
	//***************************************************/

	return true;
}

void application::onlostdevice() {
	application_error_hr(_fx->OnLostDevice());
	application_error_hr(m_font->OnLostDevice());
}

void application::onresetdevice() {
	application_error_hr(_fx->OnResetDevice());
	application_error_hr(m_font->OnResetDevice());
	application_error_hr(_fx->SetTechnique(_api_manager->m_htech) );

	/*resize causes reset, so update projection matrix */
	float w = (float)_api_manager->m_d3dpp.BackBufferWidth;
	float h = (float)_api_manager->m_d3dpp.BackBufferHeight;
	m_projection = _perspectivefovrh(D3DX_PI * 0.25f, w,h, 1.0f, 1000.0f);
	/***************************************************/
}

bool application::readrtmeshfile(const char* path,_mesh * mesh){

	FILE * file = NULL;
	fopen_s(&file,path,"rb");
	if(!file){ application_throw("readrtmeshfile"); }

	char  header_[7];
	application_zero(header_,7);
	/* file marked with 6 bytes ascii string _mesh_ */
	if( fread(header_,1,6,file) != 6 ){ application_throw("fread"); }
	if(!application_scm(header_,"_mesh_")) { application_throw("not _mesh_ file"); }

	uint16_t submesh_count_ = 0;
	/* read 2 byte unsinged int (submesh count )*/
	if( fread((&submesh_count_),2,1,file) != 1 ){ application_throw("fread"); }
	printf("smcount : %u \n",submesh_count_);

	for(uint32_t i=0;i<submesh_count_;i++){

		_submesh submesh_;

		uint32_t index_count_ = 0;
		/* read 4 byte unsinged int (vertex indicies count ) */
		if( fread((&index_count_),4,1,file) != 1 ){ application_throw("fread"); }

		int32_t * indices = new int32_t[index_count_];
		/* read indices 4 bytes each  */
		if( fread(indices,4,index_count_,file) != index_count_ ){ application_throw("fread"); }
		for(uint32_t ii=0;ii<index_count_;ii++){
			submesh_.m_indices.pushback(indices[ii],true);
		}

		uint32_t vertex_count_ = 0;
		/* read 4 byte unsinged int (vertex count ) */
		if( fread((&vertex_count_),4,1,file) != 1 ){ application_throw("fread"); }

		_vertex * vertices = new _vertex[vertex_count_];
		/* read vertices  */
		if( fread(vertices,sizeof(_vertex),vertex_count_,file) != vertex_count_ ){ application_throw("fread"); }
		for(uint32_t ii=0;ii<vertex_count_;ii++){
			submesh_.m_vertices.pushback(vertices[ii],true);
		}
		mesh->m_submeshes.pushback(submesh_,true);
	}
	uint16_t bone_count = 0;
	/* read 2 byte unsinged int (bone transform count ) */
	if( fread((&bone_count),2,1,file) != 1 ){ application_throw("fread"); }

	if(bone_count){

		printf("bone_count: %i \n",bone_count);
		mesh->m_bones.allocate(bone_count);
		/* read bone transforms  */
		if( fread( (&(mesh->m_bones[0])) ,sizeof(_mat4 ),bone_count,file) != bone_count ){ application_throw("fread"); }

		uint16_t keyframe_count = 0;
		/* read 2 byte unsinged int (animation keyframe count ) */
		if( fread( (&keyframe_count) ,2,1,file) != 1 ){ application_throw("fread"); }
		if( keyframe_count){
			printf("keyframes: %i \n",keyframe_count);
			/* read animation bone transforms  */
			for(uint32_t ii=0;ii<keyframe_count;ii++){
				_matrix_array keyframe;
				keyframe.allocate(bone_count);
				if( fread( &(keyframe[0]) ,sizeof(_mat4 ),bone_count,file) != bone_count ){ application_throw("fread"); }
				mesh->m_keyframes.pushback(keyframe);
			}
		}
	}
	fclose(file);
	return true;
}

